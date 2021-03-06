/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=cpp:et:sw=4:ts=4:sts=4

 This file is part of the Feel library

 Author(s): Thibaut Metivet <thibaut.metivet@univ-grenoble-alpes.fr>
 Date: 2016-05-04

 Copyright (C) 2016 Université Joseph Fourier (Grenoble I)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 3.0 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/**
 \file advection.hpp
 \author Thibaut Metivet <thibaut.metivet@univ-grenoble-alpes.fr>
 \date 2018-09-10
 */

#ifndef _ADVECTION_HPP
#define _ADVECTION_HPP 1

#include <feel/feeldiscr/functionspace.hpp>
#include <feel/feelfilters/exporter.hpp>
#include <feel/feelts/bdf.hpp>
#include <feel/feelvf/vf.hpp>

#include <feel/feelmodels/modelcore/modelnumerical.hpp>
#include <feel/feelmodels/modelcore/markermanagement.hpp>
#include <feel/feelmodels/modelcore/options.hpp>
#include <feel/feelmodels/modelcore/utils.hpp>
#include <feel/feelmodels/modelalg/modelalgebraicfactory.hpp>
#include <feel/feelmodels/advection/diffusionreactionmodel.hpp>

#include <feel/feelmodels/modelcore/stabilizationglsparameterbase.hpp>

namespace Feel {
namespace FeelModels {

enum class AdvectionStabMethod { NONE=0, GALS, CIP, SUPG, SGS };

namespace ADRTypes {
    using Advection = hana::integral_constant< int, (1 << 0) >;
    using Diffusion = hana::integral_constant< int, (1 << 1) >;
    using Reaction  = hana::integral_constant< int, (1 << 2) >;
    using AdvectionDiffusion = hana::integral_constant< int, (1 << 0) | (1 << 1) >;
    using AdvectionReaction = hana::integral_constant< int, (1 << 0) | (1 << 2) >;
    using AdvectionDiffusionReaction = hana::integral_constant< int, (1 << 0) | (1 << 1) | (1 << 2) >;
};

template< 
    typename FunctionSpaceType,
    typename FunctionSpaceAdvectionVelocityType = FunctionSpace< 
        typename FunctionSpaceType::mesh_type, 
        bases<Lagrange<FunctionSpaceType::basis_type::nOrder, Vectorial, Continuous, PointSetFekete>>, 
        typename FunctionSpaceType::periodicity_type >,
    typename BasisDiffusionCoeffType = Lagrange<FunctionSpaceType::basis_type::nOrder, Scalar, Continuous, PointSetFekete>,
    typename BasisReactionCoeffType = Lagrange<FunctionSpaceType::basis_type::nOrder, Scalar, Continuous, PointSetFekete>
        >
class AdvDiffReac : 
    public ModelNumerical,
    public MarkerManagementDirichletBC,
    public MarkerManagementNeumannBC,
    public std::enable_shared_from_this< AdvDiffReac<FunctionSpaceType, FunctionSpaceAdvectionVelocityType, BasisDiffusionCoeffType, BasisReactionCoeffType> >
{
public :
    typedef ModelNumerical super_type;

    typedef AdvDiffReac< FunctionSpaceType, FunctionSpaceAdvectionVelocityType, BasisDiffusionCoeffType, BasisReactionCoeffType > self_type;
    typedef std::shared_ptr<self_type> self_ptrtype;

    // ADR types
    enum ADREnum: uint16_type {
        Advection = (1 << 0),
        Diffusion = (1 << 1),
        Reaction  = (1 << 2),
        AdvectionDiffusion = Advection | Diffusion,
        AdvectionReaction = Advection | Reaction,
        DiffusionReaction = Diffusion | Reaction,
        AdvectionDiffusionReaction = Advection | Diffusion | Reaction
    };

    //--------------------------------------------------------------------//
    // Mesh
    typedef typename FunctionSpaceType::mesh_type mesh_type;
    typedef std::shared_ptr<mesh_type> mesh_ptrtype;
    typedef typename mesh_type::shape_type convex_type;
    static const uint16_type nDim = convex_type::nDim;
    static const uint16_type nOrderGeo = convex_type::nOrder;
    static const uint16_type nRealDim = convex_type::nRealDim;

    //--------------------------------------------------------------------//
    // Space advection
    typedef FunctionSpaceType space_advection_type;
    typedef std::shared_ptr<space_advection_type> space_advection_ptrtype;

    typedef typename space_advection_type::basis_type basis_advection_type;
    static const uint16_type nOrder = basis_advection_type::nOrder;

    typedef typename boost::remove_reference<
        typename fusion::result_of::at_c<typename space_advection_type::periodicity_type, 0>::type
        >::type periodicity_type;
    
    typedef typename space_advection_type::element_type element_advection_type;
    typedef std::shared_ptr<element_advection_type> element_advection_ptrtype;

    typedef typename space_advection_type::value_type value_type;
    typedef typename space_advection_type::periodicity_type periodicity_advection_type;

    static constexpr bool is_scalar = space_advection_type::is_scalar;
    static constexpr bool is_vectorial = space_advection_type::is_vectorial;
    static constexpr bool is_continuous = space_advection_type::is_continuous;

    //--------------------------------------------------------------------//
    // Space advection velocity
    //typedef Lagrange<nOrder, Vectorial, Continuous, PointSetFekete> basis_vectorial_type;
    //typedef bases<basis_vectorial_type> basis_advection_velocity_type;
    //typedef FunctionSpace< 
        //mesh_type, 
        //basis_advection_velocity_type, 
        //value_type, 
        //periodicity_advection_type > space_advection_velocity_type;
    typedef FunctionSpaceAdvectionVelocityType space_advection_velocity_type;
    typedef std::shared_ptr<space_advection_velocity_type> space_advection_velocity_ptrtype;
    typedef typename space_advection_velocity_type::element_type element_advection_velocity_type;
    typedef std::shared_ptr<element_advection_velocity_type> element_advection_velocity_ptrtype;

    //--------------------------------------------------------------------//
    // Space P0d
    typedef Lagrange<0, Scalar, Discontinuous, PointSetFekete> basis_P0d_type;
    typedef FunctionSpace< 
        mesh_type, 
        bases<basis_P0d_type>, 
        value_type, 
        periodicity_advection_type > space_P0d_type;
    typedef std::shared_ptr<space_P0d_type> space_P0d_ptrtype;
    typedef typename space_P0d_type::element_type element_P0d_type;
    typedef std::shared_ptr<element_P0d_type> element_P0d_ptrtype;

    //--------------------------------------------------------------------//
    // Range types
    typedef typename MeshTraits<mesh_type>::element_reference_wrapper_const_iterator element_reference_wrapper_const_iterator;
    typedef typename MeshTraits<mesh_type>::elements_reference_wrapper_type elements_reference_wrapper_type;
    typedef typename MeshTraits<mesh_type>::elements_reference_wrapper_ptrtype elements_reference_wrapper_ptrtype;
    typedef elements_reference_wrapper_t<mesh_type> range_elements_type;

    typedef typename MeshTraits<mesh_type>::face_reference_wrapper_const_iterator face_reference_wrapper_const_iterator;
    typedef typename MeshTraits<mesh_type>::faces_reference_wrapper_type faces_reference_wrapper_type;
    typedef typename MeshTraits<mesh_type>::faces_reference_wrapper_ptrtype faces_reference_wrapper_ptrtype;
    typedef faces_reference_wrapper_t<mesh_type> range_faces_type;

    //--------------------------------------------------------------------//
    // Diffusion-reaction model
    typedef BasisDiffusionCoeffType basis_diffusioncoeff_type;
    typedef BasisReactionCoeffType basis_reactioncoeff_type;
    static const uint16_type nOrderDiffusionCoeff = BasisDiffusionCoeffType::nOrder;
    static const uint16_type nOrderReactionCoeff = BasisReactionCoeffType::nOrder;
    typedef FunctionSpace< mesh_type, bases<basis_diffusioncoeff_type> > space_diffusioncoeff_type;
    typedef std::shared_ptr<space_diffusioncoeff_type> space_diffusioncoeff_ptrtype;
    typedef FunctionSpace< mesh_type, bases<basis_reactioncoeff_type> > space_reactioncoeff_type;
    typedef std::shared_ptr<space_reactioncoeff_type> space_reactioncoeff_ptrtype;

    typedef DiffusionReactionModel<space_diffusioncoeff_type, space_reactioncoeff_type> diffusionreaction_model_type;
    typedef std::shared_ptr<diffusionreaction_model_type> diffusionreaction_model_ptrtype;

    //--------------------------------------------------------------------//
    // Backend
    typedef Backend<value_type> backend_type;
    typedef std::shared_ptr<backend_type> backend_ptrtype;
    typedef typename backend_type::sparse_matrix_ptrtype sparse_matrix_ptrtype;
    typedef typename backend_type::vector_ptrtype vector_ptrtype;

    //--------------------------------------------------------------------//
    // Algebraic tools
    typedef ModelAlgebraicFactory model_algebraic_factory_type;
    typedef std::shared_ptr< model_algebraic_factory_type > model_algebraic_factory_ptrtype;
    typedef typename model_algebraic_factory_type::graph_type graph_type;
    typedef typename model_algebraic_factory_type::graph_ptrtype graph_ptrtype;
    typedef typename model_algebraic_factory_type::indexsplit_type indexsplit_type;
    typedef typename model_algebraic_factory_type::indexsplit_ptrtype indexsplit_ptrtype;

    //--------------------------------------------------------------------//
    // Time
    typedef Bdf<space_advection_type> bdf_type;
    typedef std::shared_ptr<bdf_type> bdf_ptrtype;

    //--------------------------------------------------------------------//
    // Exporter
    typedef Exporter<mesh_type, nOrderGeo> exporter_type;
    typedef std::shared_ptr<exporter_type> exporter_ptrtype;

    //--------------------------------------------------------------------//
    typedef map_scalar_field<2> map_scalar_field_type;
    typedef map_vector_field<nDim, 1, 2> map_vector_field_type;
    typedef typename mpl::if_< mpl::bool_<is_vectorial>,
                               map_vector_field_type,
                               map_scalar_field_type
                               >::type bc_map_field_type;
    // stabilization
    typedef StabilizationGLSParameterBase<mesh_type> stab_gls_parameter_type;
    typedef std::shared_ptr<stab_gls_parameter_type> stab_gls_parameter_ptrtype;
    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//

    //--------------------------------------------------------------------//
    // Constructor
    AdvDiffReac( 
            std::string const& prefix,
            worldcomm_ptr_t const& _worldComm = Environment::worldCommPtr(),
            std::string const& subPrefix = "",
            ModelBaseRepository const& modelRep = ModelBaseRepository() );

    AdvDiffReac( self_type const& A ) = default;

    static self_ptrtype New( 
            std::string const& prefix,
            worldcomm_ptr_t const& _worldComm = Environment::worldCommPtr(),
            std::string const& subPrefix = "",
            ModelBaseRepository const& modelRep = ModelBaseRepository() );

    //--------------------------------------------------------------------//
    // Initialization
    void init( bool buildModelAlgebraicFactory = true );

    void createMesh();
    void initFunctionSpaces();
    void initAlgebraicData();
    void initTimeDiscretization();
    void initExporters();
    void initOthers();

    void initInitialValue();

    //--------------------------------------------------------------------//
    // Mesh
    virtual std::string fileNameMeshPath() const { return prefixvm(this->prefix(),"AdvectionMesh.path"); }

    mesh_ptrtype const& mesh() const { return M_mesh; }
    void loadMesh( mesh_ptrtype m );
    void setMesh( mesh_ptrtype const& m );

    //--------------------------------------------------------------------//
    // Ranges
    range_elements_type const& rangeMeshElements() const { return M_rangeMeshElements; }

    //--------------------------------------------------------------------//
    // Periodicity
    void setPeriodicity( periodicity_type const& p );
    periodicity_type const& periodicity() const { return M_periodicity; }

    //--------------------------------------------------------------------//
    // Initial value
    void setInitialValue( bc_map_field_type const& icVal ) { M_icValue = icVal; }
    void setInitialValue( std::initializer_list<typename bc_map_field_type::value_type> icVal ) { M_icValue = icVal; }
    void setInitialValue( typename bc_map_field_type::mapped_type const& expr, std::string const& mark = "" ) { M_icValue[mark] = expr; }
    element_advection_ptrtype const& initialValue() const { return M_initialValue; }
    void setInitialValue( element_advection_ptrtype const& ival ) { M_initialValue = ival; }

    //--------------------------------------------------------------------//
    // Model and solver
    std::string const& modelName() const { return M_modelName; }
    void setModelName( std::string const& type );

    bool hasAdvection() const;
    bool hasDiffusion() const;
    bool hasReaction() const;

    std::string const& solverName() const { return M_solverName; }
    void setSolverName( std::string const& type );

    //--------------------------------------------------------------------//
    // Spaces
    space_advection_ptrtype const& functionSpace() const { return M_Xh; }
    void setFunctionSpace( space_advection_ptrtype space );
    space_advection_velocity_ptrtype const& functionSpaceAdvectionVelocity() const { return M_XhAdvectionVelocity; }
    void setFunctionSpaceAdvectionVelocity( space_advection_velocity_ptrtype space );
    space_P0d_ptrtype const& functionSpaceP0d() const { return M_spaceP0d; }
    space_diffusioncoeff_ptrtype const& functionSpaceDiffusionCoeff() const { return this->diffusionReactionModel()->functionSpaceDiffusion(); }
    space_reactioncoeff_ptrtype const& functionSpaceReactionCoeff() const { return this->diffusionReactionModel()->functionSpaceReaction(); }
    
    bool useExtendedDofTable() const;

    element_advection_ptrtype & fieldSolutionPtr() { return M_fieldSolution; }
    element_advection_ptrtype const& fieldSolutionPtr() const { return M_fieldSolution; }
    element_advection_type & fieldSolution() { return *M_fieldSolution; }
    element_advection_type const& fieldSolution() const { return *M_fieldSolution; }

    element_advection_velocity_type const& fieldAdvectionVelocity() const { return *M_fieldAdvectionVelocity; }
    element_advection_velocity_ptrtype const& fieldAdvectionVelocityPtr() const { return M_fieldAdvectionVelocity; }

    //--------------------------------------------------------------------//
    // Algebraic data
    backend_ptrtype const& backend() const { return M_backend; }
    size_type matrixPattern() const;
    virtual int nBlockMatrixGraph() const;
    virtual BlocksBaseGraphCSR buildBlockMatrixGraph() const;
    graph_ptrtype buildMatrixGraph() const;
    virtual int buildBlockVectorSolution();
    void buildVectorSolution();
    //indexsplit_ptrtype buildIndexSplit() const;
    model_algebraic_factory_ptrtype & algebraicFactory() { return M_algebraicFactory; }
    model_algebraic_factory_ptrtype const& algebraicFactory() const { return M_algebraicFactory; }
    virtual size_type nLocalDof() const;

    //--------------------------------------------------------------------//
    // Time scheme
    bdf_ptrtype timeStepBDF() { return M_bdf; }
    bdf_ptrtype const& timeStepBDF() const { return M_bdf; }
    std::shared_ptr<TSBase> timeStepBase() { return this->timeStepBDF(); }
    std::shared_ptr<TSBase> timeStepBase() const { return this->timeStepBDF(); }
    virtual void updateTimeStepBDF();
    void updateTimeStep() { this->updateTimeStepBDF(); }
    void initTimeStep();

    //--------------------------------------------------------------------//
    // Stabilization
    AdvectionStabMethod stabilizationMethod() const { return M_stabMethod; }
    stab_gls_parameter_ptrtype const& stabilizationGLSParameter() const { return M_stabilizationGLSParameter; }
    double stabilizationCIPCoefficient() const { return M_stabilizationCIPCoefficient; }
    void setStabilizationCIPCoefficient(double coeff) { M_stabilizationCIPCoefficient = coeff; }

    //--------------------------------------------------------------------//
    // Algebraic model updates
    // Linear PDE
    void updateLinearPDE( DataUpdateLinear & data ) const;
    virtual void updateLinearPDEAdditional( sparse_matrix_ptrtype & A, vector_ptrtype & F, bool _BuildCstPart ) const {}
    virtual void updateLinearPDEStabilization( DataUpdateLinear & data ) const;
    virtual void updateSourceTermLinearPDE( DataUpdateLinear & data ) const;
    virtual bool hasSourceTerm() const;
    virtual void updateWeakBCLinearPDE(sparse_matrix_ptrtype& A, vector_ptrtype& F,bool buildCstPart) const;
    virtual void updateBCStrongDirichletLinearPDE(sparse_matrix_ptrtype& A, vector_ptrtype& F) const;
    
    void updateBCNeumannLinearPDE( vector_ptrtype& F ) const;
    
    //--------------------------------------------------------------------//
    // Advection velocity update
    void updateAdvectionVelocity( element_advection_velocity_ptrtype const& u );
    void updateAdvectionVelocity( element_advection_velocity_type const& u );
    template<typename ExprT>
    void updateAdvectionVelocity(vf::Expr<ExprT> const& expr);
    //--------------------------------------------------------------------//
    // Volumic sources
    bc_map_field_type const& volumicSources() const { return M_sources; }
    //--------------------------------------------------------------------//
    // Source update
    template<typename ExprT>
    void updateSourceAdded(vf::Expr<ExprT> const& expr);
    bool hasSourceAdded() const { return M_hasSourceAdded; }
    element_advection_ptrtype const& sourceAdded() const { return M_fieldSource; }
    //--------------------------------------------------------------------//
    // Diffusion-reaction parameters update
    diffusionreaction_model_ptrtype & diffusionReactionModel() { return M_diffusionReactionModel; }
    diffusionreaction_model_ptrtype const& diffusionReactionModel() const { return M_diffusionReactionModel; }

    void updateDiffusionCoeff(double D)
    {
        this->diffusionReactionModel()->setCstDiffusionCoeff(D);
    }
    void updateReactionCoeff(double R)
    {
        this->diffusionReactionModel()->setCstReactionCoeff(R);
    }

    template<typename ExprT>
    void updateDiffusionCoeff(vf::Expr<ExprT> const& expr)
    {
        this->diffusionReactionModel()->updateDiffusionCoeff(expr);
    }
    template<typename ExprT>
    void updateReactionCoeff(vf::Expr<ExprT> const& expr)
    {
        this->diffusionReactionModel()->updateReactionCoeff(expr);
    }

    //--------------------------------------------------------------------//
    // Solve
    virtual void solve();

    //--------------------------------------------------------------------//
    // Export results
    void initPostProcess();
    void exportResults() { this->exportResults( this->currentTime() ); }
    void exportResults( double time );
    void exportMeasures( double time );

    exporter_ptrtype getExporter() { return M_exporter; }
    exporter_ptrtype const& getExporter() const { return M_exporter; }

    //--------------------------------------------------------------------//
    void addMarkerInflowBC( std::string const& markerName );

    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
protected:
    void loadParametersFromOptionsVm();
    void loadConfigICFile();
    void loadConfigBCFile();

    virtual void updateLinearPDETransient( sparse_matrix_ptrtype& A, vector_ptrtype& F, bool buildCstPart ) const;

    virtual std::string geoExportType() const { return "static"; }
    virtual void exportResultsImpl( double time );
    virtual void exportMeasuresImpl( double time );

    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
    //--------------------------------------------------------------------//
    bool M_isUpdatedForUse;
    //--------------------------------------------------------------------//
    // Model and solver
    std::string M_modelName;
    std::string M_solverName;
    //--------------------------------------------------------------------//
    // Mesh
    mesh_ptrtype M_mesh;
    range_elements_type M_rangeMeshElements;
    // Periodicity
    periodicity_type M_periodicity;
    // Advection space
    space_advection_ptrtype M_Xh;
    // P0d space
    space_P0d_ptrtype M_spaceP0d;
    // Time discretization
    bdf_ptrtype M_bdf;
    //--------------------------------------------------------------------//
    // Algebraic data
    backend_ptrtype M_backend;
    model_algebraic_factory_ptrtype M_algebraicFactory;
    BlocksBaseVector<double> M_blockVectorSolution;
    //--------------------------------------------------------------------//
    // Advection velocity
    space_advection_velocity_ptrtype M_XhAdvectionVelocity;
    element_advection_velocity_ptrtype M_fieldAdvectionVelocity;
    boost::optional<vector_field_expression<nDim,1,2> > M_exprAdvectionVelocity;
    //--------------------------------------------------------------------//
    // Physical parameters (diffusivity and reaction coefficient)
    diffusionreaction_model_ptrtype M_diffusionReactionModel;
    //--------------------------------------------------------------------//
    // Source added
    element_advection_ptrtype M_fieldSource;
    bool M_hasSourceAdded;
    //--------------------------------------------------------------------//
    // Solution
    element_advection_ptrtype M_fieldSolution;
    //--------------------------------------------------------------------//
    // Boundary conditions
    bc_map_field_type M_bcDirichlet;
    bc_map_field_type M_bcNeumann;
    //map_scalar_fields<2> M_bcRobin;
    std::list<std::string> M_bcInflowMarkers;
    // Initial conditions
    element_advection_ptrtype M_initialValue;
    bc_map_field_type M_icValue;
    // body forces
    bc_map_field_type M_sources;
    //--------------------------------------------------------------------//
    // Export
    exporter_ptrtype M_exporter;
    bool M_doExportAll;
    bool M_doExportAdvectionVelocity;
    bool M_doExportDiffusionCoefficient;
    bool M_doExportReactionCoefficient;
    bool M_doExportSourceField;
    //--------------------------------------------------------------------//
    // Stabilization
    static const std::map<std::string, AdvectionStabMethod> AdvectionStabMethodIdMap;
    AdvectionStabMethod M_stabMethod;
    double M_stabilizationCIPCoefficient;
    double M_gamma1;
    // stabilization
    //bool M_stabilizationGLS;
    //std::string M_stabilizationGLSType;
    stab_gls_parameter_ptrtype M_stabilizationGLSParameter;

};//AdvDiffReac

//----------------------------------------------------------------------------//
// Advection velocity update
template< 
    typename FunctionSpaceType,
    typename FunctionSpaceAdvectionVelocityType,
    typename BasisDiffusionCoeffType,
    typename BasisReactionCoeffType
        >
template<typename ExprT>
void
AdvDiffReac<FunctionSpaceType, FunctionSpaceAdvectionVelocityType, BasisDiffusionCoeffType, BasisReactionCoeffType>::updateAdvectionVelocity(
        vf::Expr<ExprT> const& v_expr)
{
    M_exprAdvectionVelocity.reset(); // remove symbolic expr
    M_fieldAdvectionVelocity->on(_range=elements(this->mesh()), _expr=v_expr );
}

//----------------------------------------------------------------------------//
// Source update
template< 
    typename FunctionSpaceType,
    typename FunctionSpaceAdvectionVelocityType,
    typename BasisDiffusionCoeffType,
    typename BasisReactionCoeffType
        >
template<typename ExprT>
void 
AdvDiffReac<FunctionSpaceType, FunctionSpaceAdvectionVelocityType, BasisDiffusionCoeffType, BasisReactionCoeffType>::updateSourceAdded(
        vf::Expr<ExprT> const& f_expr)
{
    if (!M_fieldSource)
    {
        M_fieldSource.reset( new element_advection_type(M_Xh, "SourceAdded") );
    }
    M_fieldSource->on(_range=elements( this->mesh() ), _expr=f_expr );
    M_hasSourceAdded=true;
}

} // namespace FeelModels
} // namespace Feel


#endif
