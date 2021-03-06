/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=cpp:et:sw=4:ts=4:sts=4

 This file is part of the Feel library

 Author(s): Vincent Chabannes <vincent.chabannes@feelpp.org>
 Date: 2012-01-19

 Copyright (C) 2011 Université Joseph Fourier (Grenoble I)

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
 \file modelalgebraic.hpp
 \author Vincent Chabannes <vincent.chabannes@feelpp.org>
 \date 2012-01-19
 */

#ifndef FEELPP_MODELALGEBRAIC_HPP
#define FEELPP_MODELALGEBRAIC_HPP 1

#include <feel/feelmodels/modelcore/modelbase.hpp>

#include <feel/feelalg/backend.hpp>
#include <feel/feelalg/vectorblock.hpp>
#include <feel/feelmesh/enums.hpp>

namespace Feel
{
namespace FeelModels
{

class ModelAlgebraic : public ModelBase
{
public :
    typedef ModelBase super_type;

    typedef double value_type;
    typedef Backend<value_type> backend_type;
    typedef std::shared_ptr<backend_type> backend_ptrtype;

    typedef backend_type::sparse_matrix_ptrtype sparse_matrix_ptrtype;
    typedef backend_type::vector_ptrtype vector_ptrtype;

    typedef backend_type::graph_type graph_type;
    typedef backend_type::graph_ptrtype graph_ptrtype;
    typedef backend_type::indexsplit_type indexsplit_type;
    typedef backend_type::indexsplit_ptrtype indexsplit_ptrtype;

    typedef vf::BlocksBase<size_type> block_pattern_type;

    class DataUpdateBase
    {
    public:
        DataUpdateBase() = default;
        DataUpdateBase( DataUpdateBase const& ) = default;
        DataUpdateBase( DataUpdateBase && ) = default;
        void addInfo( std::string const& info ) { M_infos.insert( info ); }
        bool hasInfo( std::string const& info ) const { return M_infos.find( info ) != M_infos.end(); }
        void addDoubleInfo( std::string const& info, double val ) { M_doubleInfos[info] = val; }
        bool hasDoubleInfo( std::string const& info ) const { return M_doubleInfos.find( info ) != M_doubleInfos.end(); }
        double doubleInfo( std::string const& info ) const
            {
                CHECK( this->hasDoubleInfo( info ) ) << "double info "<< info << "is missing";
                return M_doubleInfos.find(info)->second;
            }
    private :
        std::set<std::string> M_infos;
        std::map<std::string,double> M_doubleInfos;
    };

    class DataUpdateLinear : public DataUpdateBase
    {
    public:
        DataUpdateLinear( const vector_ptrtype& currentSolution,
                          sparse_matrix_ptrtype matrix, vector_ptrtype rhs,
                          bool buildCstPart,
                          sparse_matrix_ptrtype matrixExtended, bool buildExtendedPart )
            :
            DataUpdateBase(),
            M_matrix( matrix ),
            M_rhs( rhs ),
            M_currentSolution( currentSolution ),
            M_buildCstPart( buildCstPart ),
            M_matrixExtended( matrixExtended ),
            M_buildExtendedPart( buildExtendedPart ),
            M_doBCStrongDirichlet( true )
            {}
        DataUpdateLinear(DataUpdateLinear const& d) = default;
        DataUpdateLinear(DataUpdateLinear && d) = default;

        sparse_matrix_ptrtype& matrix() { return M_matrix; }
        vector_ptrtype& rhs() { return M_rhs; }
        vector_ptrtype const& currentSolution() { return M_currentSolution; }
        bool buildCstPart() const { return M_buildCstPart; }
        sparse_matrix_ptrtype& matrixExtended() { return M_matrixExtended; }
        bool buildExtendedPart() const { return M_buildExtendedPart; }
        bool doBCStrongDirichlet() const { return M_doBCStrongDirichlet; }
        std::map<Feel::MatrixStructure,std::pair<sparse_matrix_ptrtype,double>> const& matrixToAdd() const { return M_matrixToAdd; }
        std::vector<std::pair<sparse_matrix_ptrtype,vector_ptrtype>> const& rhsToAddFromMatrixVectorProduct() const { return M_rhsToAddFromMatrixVectorProduct; }

        void setBuildCstPart( bool b ) { M_buildCstPart = b; }
        void setDoBCStrongDirichlet( bool b ){ M_doBCStrongDirichlet = b; }
        void addMatrixToAdd( sparse_matrix_ptrtype mat, Feel::MatrixStructure matStruc, double scaling ) { M_matrixToAdd[matStruc] = std::make_pair( mat, scaling ); }
        void addRhsToAdd( sparse_matrix_ptrtype mat, vector_ptrtype vec ) { M_rhsToAddFromMatrixVectorProduct.push_back( std::make_pair(mat,vec) ); }
    private :
        sparse_matrix_ptrtype M_matrix;
        vector_ptrtype M_rhs;
        const vector_ptrtype& M_currentSolution;

        bool M_buildCstPart;
        sparse_matrix_ptrtype M_matrixExtended;
        bool M_buildExtendedPart;
        bool M_doBCStrongDirichlet;

        std::map<Feel::MatrixStructure,std::pair<sparse_matrix_ptrtype,double>> M_matrixToAdd;
        std::vector<std::pair<sparse_matrix_ptrtype,vector_ptrtype>> M_rhsToAddFromMatrixVectorProduct;
    };

    class DataUpdateResidual : public DataUpdateBase
    {
    public:
        DataUpdateResidual( const vector_ptrtype& currentSolution, vector_ptrtype residual,
                            bool buildCstPart, bool useJacobianLinearTerms )
            :
            DataUpdateBase(),
            M_residual( residual ),
            M_currentSolution( currentSolution ),
            M_buildCstPart( buildCstPart ),
            M_useJacobianLinearTerms( useJacobianLinearTerms ),
            M_doBCStrongDirichlet( true )
            {}
        DataUpdateResidual( DataUpdateResidual const& d ) = default;
        DataUpdateResidual( DataUpdateResidual && d ) = default;

        vector_ptrtype& residual() { return M_residual; }
        vector_ptrtype const& currentSolution() { return M_currentSolution; }
        bool buildCstPart() const { return M_buildCstPart; }
        bool useJacobianLinearTerms() const { return M_useJacobianLinearTerms; }
        bool doBCStrongDirichlet() const { return M_doBCStrongDirichlet; }

        void setBuildCstPart( bool b ) { M_buildCstPart = b; }
        void setDoBCStrongDirichlet( bool b ){ M_doBCStrongDirichlet = b; }

    private :
        vector_ptrtype M_residual;
        const vector_ptrtype& M_currentSolution;
        bool M_buildCstPart;
        bool M_useJacobianLinearTerms;
        bool M_doBCStrongDirichlet;
    };

    class DataUpdateJacobian : public DataUpdateBase
    {
    public:
        DataUpdateJacobian( const vector_ptrtype& currentSolution, sparse_matrix_ptrtype jacobian,
                            vector_ptrtype vectorUsedInStrongDirichlet, bool buildCstPart,
                            sparse_matrix_ptrtype matrixExtended, bool buildExtendedPart )
            :
            DataUpdateBase(),
            M_jacobian( jacobian ),
            M_vectorUsedInStrongDirichlet( vectorUsedInStrongDirichlet ),
            M_currentSolution( currentSolution ),
            M_buildCstPart( buildCstPart ),
            M_matrixExtended( matrixExtended ),
            M_buildExtendedPart( buildExtendedPart ),
            M_doBCStrongDirichlet( true )
            {}

        DataUpdateJacobian( DataUpdateJacobian const& d) = default;
        DataUpdateJacobian( DataUpdateJacobian && d) = default;

        sparse_matrix_ptrtype& jacobian() { return M_jacobian; }
        vector_ptrtype& vectorUsedInStrongDirichlet() { return M_vectorUsedInStrongDirichlet; }
        vector_ptrtype const& currentSolution() { return M_currentSolution; }

        bool buildCstPart() const { return M_buildCstPart; }
        sparse_matrix_ptrtype& matrixExtended() { return M_matrixExtended; }
        bool buildExtendedPart() const { return M_buildExtendedPart; }
        bool doBCStrongDirichlet() const { return M_doBCStrongDirichlet; }

        void setBuildCstPart( bool b ) { M_buildCstPart = b; }
        void setDoBCStrongDirichlet( bool b ){ M_doBCStrongDirichlet = b; }

    private :
        sparse_matrix_ptrtype M_jacobian;
        vector_ptrtype M_vectorUsedInStrongDirichlet;
        const vector_ptrtype& M_currentSolution;
        bool M_buildCstPart;
        sparse_matrix_ptrtype M_matrixExtended;
        bool M_buildExtendedPart;
        bool M_doBCStrongDirichlet;
    };

    class DataDofIdsMultiProcessModified
    {
    public :
        DataDofIdsMultiProcessModified() = default;
        DataDofIdsMultiProcessModified( DataDofIdsMultiProcessModified const& d) = default;
        DataDofIdsMultiProcessModified( DataDofIdsMultiProcessModified && d) = default;

        bool hasDofIdsMultiProcessModified( ElementsType e ) const { return M_dofIdsMultiProcessModified.find( e ) != M_dofIdsMultiProcessModified.end(); }
        bool hasDofIdsMultiProcessModified() const
            {
                return this->hasDofIdsMultiProcessModified( MESH_ELEMENTS ) ||
                    this->hasDofIdsMultiProcessModified( MESH_FACES ) ||
                    this->hasDofIdsMultiProcessModified( MESH_EDGES ) ||
                    this->hasDofIdsMultiProcessModified( MESH_POINTS );
            }
        std::set<size_type> const& dofIdsMultiProcessModified( ElementsType e ) const
            {
                CHECK( this->hasDofIdsMultiProcessModified(e) ) << "entity not registered";
                return M_dofIdsMultiProcessModified.find( e )->second;
            }
        std::set<size_type> & dofIdsMultiProcessModified( ElementsType e ) { return M_dofIdsMultiProcessModified[e]; }
        void initDofIdsMultiProcessModified( ElementsType e ) { M_dofIdsMultiProcessModified[e]; }
        void addDofIdsMultiProcessModified( ElementsType e, size_type id ) { M_dofIdsMultiProcessModified[e].insert( id ); }
        void addDofIdsMultiProcessModified( ElementsType e, std::set<size_type> const& ids ) { M_dofIdsMultiProcessModified[e].insert( ids.begin(), ids.end() ); }
    private:
        std::map<ElementsType,std::set<size_type>> M_dofIdsMultiProcessModified;
    };

    class DataNewtonInitialGuess : public DataUpdateBase, public DataDofIdsMultiProcessModified
    {
    public:
        DataNewtonInitialGuess( vector_ptrtype& initialGuess )
            :
            DataUpdateBase(),
            DataDofIdsMultiProcessModified(),
            M_initialGuess( initialGuess )
            {}
        DataNewtonInitialGuess( DataNewtonInitialGuess const& d) = default;
        DataNewtonInitialGuess( DataNewtonInitialGuess && d) = default;
        vector_ptrtype & initialGuess() { return M_initialGuess; }


    private:
        vector_ptrtype& M_initialGuess;
    };

    ModelAlgebraic( std::string _theprefix,
                    worldcomm_ptr_t const& _worldComm=Environment::worldCommPtr(),
                    std::string const& subPrefix="",
                    ModelBaseRepository const& modelRep = ModelBaseRepository() );

    ModelAlgebraic( ModelAlgebraic const& app ) = default;

    virtual ~ModelAlgebraic();


    // verbose
    bool verboseSolverTimer() const;
    bool verboseSolverTimerAllProc() const;
    // do rebuild cst part in linear/jacobian or use jac for residual
    bool rebuildCstPartInLinearSystem() const;
    void setRebuildCstPartInLinearSystem(bool b);
    bool useLinearJacobianInResidual() const;
    void setUseLinearJacobianInResidual(bool b);
    bool rebuildLinearPartInJacobian() const;
    void setRebuildLinearPartInJacobian(bool b);
    // a utiliser avec precaution!!!
    bool rebuildCstPartInResidual() const;
    void setRebuildCstPartInResidual(bool b);
    // define an other matrix/vector to store the cst part
    bool useCstMatrix() const;
    void setUseCstMatrix(bool b);
    bool useCstVector() const;
    void setUseCstVector(bool b);
    // allow to rebuild cst part (once at next solve) if some parameters (model,time mode,..) change
    bool needToRebuildCstPart() const;
    void setNeedToRebuildCstPart(bool b);
    // an option
    bool errorIfSolverNotConverged() const;
    void setErrorIfSolverNotConverged( bool b );
    // save a python script to view graph
    bool printGraph() const;
    void setPrintGraph(bool b);
    std::string printGraphFileName() const;
    void setPrintGraphFileName(std::string s);

    //----------------------------------------------------------------------------------//
    /**
     * return false
     */
    virtual bool hasExtendedPattern() const;

    /**
     * return an empty blockPattern if not overhead
     */
    virtual block_pattern_type blockPattern() const;

    bool buildMatrixPrecond() const;

    virtual
    void
    updatePreconditioner(const vector_ptrtype& X,
                         sparse_matrix_ptrtype& A,
                         sparse_matrix_ptrtype& A_extended,
                         sparse_matrix_ptrtype& Prec) const;

    virtual void updateInHousePreconditioner( sparse_matrix_ptrtype const& mat, vector_ptrtype const& vecSol ) const;

    virtual BlocksBaseGraphCSR buildBlockMatrixGraph() const;
    virtual graph_ptrtype buildMatrixGraph() const;

    //----------------------------------------------------------------------------------//

    virtual void updateNewtonInitialGuess( DataNewtonInitialGuess & data ) const;
    virtual void updateJacobian( DataUpdateJacobian & data ) const;
    virtual void updateJacobianDofElimination( DataUpdateJacobian & data ) const;
    virtual void updateResidual( DataUpdateResidual & data ) const;
    virtual void updateResidualDofElimination( DataUpdateResidual & data ) const;
    virtual void updateLinearPDE( DataUpdateLinear & data ) const;
    virtual void updateLinearPDEDofElimination( DataUpdateLinear & data ) const;
    virtual void updatePicard( DataUpdateLinear & data ) const;
    virtual double updatePicardConvergence( vector_ptrtype const& Unew, vector_ptrtype const& Uold ) const;

    //----------------------------------------------------------------------------------//
    virtual void preSolveNewton( vector_ptrtype rhs, vector_ptrtype sol ) const {}
    virtual void postSolveNewton( vector_ptrtype rhs, vector_ptrtype sol ) const {}
    virtual void preSolvePicard( vector_ptrtype rhs, vector_ptrtype sol ) const {}
    virtual void postSolvePicard( vector_ptrtype rhs, vector_ptrtype sol ) const {}
    virtual void preSolveLinear( vector_ptrtype rhs, vector_ptrtype sol ) const {}
    virtual void postSolveLinear( vector_ptrtype rhs, vector_ptrtype sol ) const {}
    //----------------------------------------------------------------------------------//
    virtual void updateNewtonIteration( int step, vector_ptrtype residual, vector_ptrtype sol, typename backend_type::solvernonlinear_type::UpdateIterationData const& data ) const {}


    //! index start of (sub-)block
    size_type rowStartInMatrix() const { return this->startBlockSpaceIndexMatrixRow(); }
    size_type colStartInMatrix() const { return this->startBlockSpaceIndexMatrixCol(); }
    size_type rowStartInVector() const { return this->startBlockSpaceIndexVector(); }
    size_type startBlockSpaceIndexMatrixRow() const { return M_startBlockSpaceIndexMatrixRow; }
    size_type startBlockSpaceIndexMatrixCol() const { return M_startBlockSpaceIndexMatrixCol; }
    size_type startBlockSpaceIndexVector() const { return M_startBlockSpaceIndexVector; }
    void setStartBlockSpaceIndexMatrixRow( size_type s ) { M_startBlockSpaceIndexMatrixRow = s; }
    void setStartBlockSpaceIndexMatrixCol( size_type s ) { M_startBlockSpaceIndexMatrixCol = s; }
    void setStartBlockSpaceIndexVector( size_type s ) { M_startBlockSpaceIndexVector = s; }
    void setStartBlockSpaceIndex( size_type s ) { this->setStartBlockSpaceIndexMatrixRow( s ); this->setStartBlockSpaceIndexMatrixCol( s ); this->setStartBlockSpaceIndexVector( s ); }

    size_type startSubBlockSpaceIndex( std::string const& name ) const
        {
            auto itFind = M_startSubBlockSpaceIndex.find( name );
            if ( itFind != M_startSubBlockSpaceIndex.end() )
                return itFind->second;
            return invalid_size_type_value;
        }
    bool hasStartSubBlockSpaceIndex( std::string const& name ) const { return (this->startSubBlockSpaceIndex( name ) != invalid_size_type_value); }
    void setStartSubBlockSpaceIndex( std::string const& name, size_type s ) { M_startSubBlockSpaceIndex[name] = s; }

    //! update data usefull for mpi synchronization of NewtonInitialGuess
    void updateDofEliminationIdsMultiProcess( std::string const& spaceName, DataNewtonInitialGuess & data ) const;
    void updateDofEliminationIdsMultiProcess( std::string const& spaceName, std::map<ElementsType, std::set<size_type>> const& dofIdsMultiProcess, DataNewtonInitialGuess & data ) const;
    std::set<size_type> & dofEliminationIdsMultiProcess( std::string const& spaceName, ElementsType e ) { return M_dofEliminationIdsMultiProcess[spaceName][e]; }
    std::map<std::string,std::map<ElementsType, std::set<size_type> > > const& dofEliminationIdsMultiProcess() const { return M_dofEliminationIdsMultiProcess; }
    std::map<ElementsType, std::set<size_type> > const& dofEliminationIdsMultiProcess( std::string const& spaceName ) const
        {
            CHECK( this->hasDofEliminationIdsMultiProcess( spaceName ) ) << "no space name registered : " << spaceName;
            return M_dofEliminationIdsMultiProcess.find( spaceName )->second;
        }
    bool hasDofEliminationIdsMultiProcess( std::string const& spaceName ) const { return M_dofEliminationIdsMultiProcess.find( spaceName ) != M_dofEliminationIdsMultiProcess.end(); }

private :
    // verbose
    bool M_verboseSolverTimer,M_verboseSolverTimerAllProc;

    bool M_rebuildCstPartInLinearSystem;
    bool M_useLinearJacobianInResidual;
    bool M_rebuildLinearPartInJacobian;
    bool M_rebuildCstPartInResidual;
    bool M_useCstMatrix,M_useCstVector;
    bool M_needToRebuildCstPart;

    bool M_errorIfSolverNotConverged;
    // save a python script to view graph
    bool M_printGraph;
    std::string M_printGraphFileName;

    //! index start of (sub-)block
    size_type M_startBlockSpaceIndexMatrixRow, M_startBlockSpaceIndexMatrixCol, M_startBlockSpaceIndexVector;
    std::map<std::string,size_type> M_startSubBlockSpaceIndex;
    //! dofs eliminiation
    std::map<std::string,std::map<ElementsType, std::set<size_type> > > M_dofEliminationIdsMultiProcess;


};


} // namespace FeelModels
} // namespace feel


#endif // FEELPP_MODELALGEBRAIC_HPP
