/**
 * \copyright
 * Copyright (c) 2015, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 *
 */

/*
   The members of class Element definitions.
   Designed and programmed by WW, 06/2004
 */

//#include "makros.h"
//#include <iostream>
#include "fem_ele_std.h"
#include <cfloat>
/* Objekte */
#include "rf_pcs.h"

#include "femlib.h"
#include "mathlib.h"
//#include "matrix_class.h"
// MSHLib
//#include "msh_elem.h"
// Will be removed when new FEM is ready
//=============================================
FiniteElement::CElement* elem_dm = NULL;
//=============================================

namespace FiniteElement
{
/**************************************************************************
   FEMLib-Method:
   Task: Constructor of class CElement
   Programing:
   01/2005 WW Implementation
   01/2005 OK 1D case
   01/2006 WW Axisymmetry
   Last modified:
**************************************************************************/
CElement::CElement(int CoordFlag, const int order)
    : MeshElement(NULL), Order(order), ele_dim(1), nGaussPoints(1), nGauss(1), ShapeFunction(NULL),
      ShapeFunctionHQ(NULL), GradShapeFunction(NULL), GradShapeFunctionHQ(NULL), T_Flag(false), C_Flag(false),
      F_Flag(false), D_Flag(0), RD_Flag(false), extrapo_method(ExtrapolationMethod::EXTRAPO_LINEAR)
{
	int i;
	//
	nGauss = 3;
	//
	if (CoordFlag < 0) // Axisymmetry
	{
		CoordFlag *= -1;
		axisymmetry = true;
	}
	else
		axisymmetry = false;
	//
	dim = CoordFlag / 10;
	coordinate_system = CoordFlag;
	for (i = 0; i < 4; i++)
		unit[i] = 0.0;
	switch (dim)
	{
		case 1: // OK
			// Memory allocated for maxium 3 nodes elements
			Jacobian = new double[1];
			invJacobian = new double[1];
			shapefct = new double[2];
			shapefctHQ = new double[3];
			dshapefct = new double[6];
			dshapefctHQ = new double[9];
			break;
		case 2:
			// Memory allocated for maxium 9 nodes elements
			Jacobian = new double[4];
			invJacobian = new double[4];
			shapefct = new double[4];
			shapefctHQ = new double[9];
			dshapefct = new double[18];
			dshapefctHQ = new double[18];
			break;
		case 3:
			// Memory allocated for maxium 20 nodes elements
			Jacobian = new double[9];
			invJacobian = new double[9];
			shapefct = new double[8];
			shapefctHQ = new double[20];
			dshapefct = new double[24];
			dshapefctHQ = new double[60];
			//
			break;
	}
	time_unit_factor = 1.0;

	if (M_Process)
		D_Flag = 4;
	if (MH_Process)
		D_Flag = 41;
	F_Flag = H_Process;
	T_Flag = T_Process;
	C_Flag = MASS_TRANSPORT_Process;
	PT_Flag = 0; // PCH Initialize to be no RWPT.
	RD_Flag = RD_Process;
	MCF_Flag = MULTI_COMPONENTIAL_FLOW_Process;

#if defined(USE_PETSC) // || defined(other parallel libs)//03~04.3012. WW
	idxm = NULL; //> global indices of local matrix rows
	idxn = NULL; //> global indices of local matrix columns
	local_idx = NULL; //> local index for local assemble
// local_matrix = NULL; //>  local matrix
// local_vec = NULL; //>  local vector
#endif
}

//  Destructor of class Element
CElement::~CElement()
{
	delete[] Jacobian;
	delete[] invJacobian;
	delete[] shapefct;
	delete[] shapefctHQ;
	delete[] dshapefct;
	delete[] dshapefctHQ;
	Jacobian = NULL;
	shapefct = NULL;
	dshapefct = NULL;
	dshapefctHQ = NULL;
	shapefctHQ = NULL;

#if defined(USE_PETSC) // || defined(other parallel libs)//03~04.3012. WW
	if (idxm)
		delete[] idxm;
	if (idxn)
		delete[] idxn;
	if (local_idx)
		delete[] local_idx;
// if (local_idx)
//  delete [] local_matrix;
// if (local_idx)
//  delete [] local_vec;
#endif
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   06/2004 WW Implementation
   05/2007 WW 1D in 2D
   Last modified:
**************************************************************************/
void CElement::ConfigElement(CElem* MElement, const int nquadrature_points, bool FaceIntegration)
{
	CNode* a_node = NULL; // 07.04.2009. WW
	MeshElement = MElement;
	Index = MeshElement->GetIndex();
	nnodes = MeshElement->nnodes;
	nnodesHQ = MeshElement->nnodesHQ;
	bool done = false;
	ConfigNumerics(MeshElement->GetElementType(), nquadrature_points);
	if (MeshElement->quadratic)
		nNodes = nnodesHQ;
	else
		nNodes = nnodes;
	// Node indices
	for (int i = 0; i < nNodes; i++)
		nodes[i] = MeshElement->nodes_index[i];
	// Put coordinates of nodes to buffer to enhance the computation
	if (!FaceIntegration)
	{
		if (dim != ele_dim)
		{
			//            a_node0 = MeshElement->nodes[0];      //07.04.2007. WW
			double const* const coords_node_0(MeshElement->nodes[0]->getData());
			for (int i = 0; i < nNodes; i++)
			{
				double const* const coords_node_i(MeshElement->nodes[i]->getData());
				//               a_node = MeshElement->nodes[i];    //07.04.2007. WW
				//               dy = dz = 0.;
				//               dx = a_node->X()-a_node0->X();
				//               dy = a_node->Y()-a_node0->Y();
				//               dz = a_node->Z()-a_node0->Z();
				double dx(coords_node_i[0] - coords_node_0[0]);
				double dy(coords_node_i[1] - coords_node_0[1]);
				double dz(coords_node_i[2] - coords_node_0[2]);

				X[i] = (*MeshElement->transform_tensor)(0, 0) * dx + (*MeshElement->transform_tensor)(1, 0) * dy
				       + (*MeshElement->transform_tensor)(2, 0) * dz;
				Y[i] = (*MeshElement->transform_tensor)(0, 1) * dx + (*MeshElement->transform_tensor)(1, 1) * dy
				       + (*MeshElement->transform_tensor)(2, 1) * dz;
				//               Z[i] =  a_node->Z();
				Z[i] = coords_node_i[2];
			}
			done = true;
		}
		else
		{
			switch (dim)
			{
				case 1:
					if (coordinate_system % 10 == 1)
					{
						for (int i = 0; i < nNodes; i++)
						{
							// 07.04.2007. WW
							//                        a_node = MeshElement->nodes[i];
							//                        X[i] = a_node->Y();
							//                        Y[i] = a_node->X();
							//                        Z[i] = a_node->Z();
							double const* const coords_node_i(MeshElement->nodes[i]->getData());
							X[i] = coords_node_i[1];
							Y[i] = coords_node_i[0];
							Z[i] = coords_node_i[2];
						}
						done = true;
					}
					else if (coordinate_system % 10 == 2)
					{
						for (int i = 0; i < nNodes; i++)
						{
							// 07.04.2007. WW
							//                        a_node = MeshElement->nodes[i];
							//                        X[i] = a_node->Z();
							//                        Y[i] = a_node->Y();
							//                        Z[i] = a_node->X();
							double const* const coords_node_i(MeshElement->nodes[i]->getData());
							X[i] = coords_node_i[2];
							Y[i] = coords_node_i[1];
							Z[i] = coords_node_i[0];
						}
						done = true;
					}
					break;
				case 2:
					if (coordinate_system % 10 == 2)
					{
						for (int i = 0; i < nNodes; i++)
						{
							// 07.04.2007. WW
							//                        a_node = MeshElement->nodes[i];
							//                        X[i] = a_node->X();
							//                        Y[i] = a_node->Z();
							//                        Z[i] = a_node->Y();
							double const* const coords_node_i(MeshElement->nodes[i]->getData());
							X[i] = coords_node_i[0];
							Y[i] = coords_node_i[2];
							Z[i] = coords_node_i[1];
						}
						done = true;
					}
					break;
			}
		}
	}
	//
	if (!done)
	{
		for (int i = 0; i < nNodes; i++)
		{
			a_node = MeshElement->nodes[i]; // 07.04.2007. WW
			double const* const coords(a_node->getData());
			X[i] = coords[0];
			Y[i] = coords[1];
			Z[i] = coords[2];
		}
	}

#if defined(USE_PETSC) // || defined(other parallel libs)//03~04.3012. WW
	if (!FaceIntegration)
	{
		if (MeshElement->g_index) // ghost nodes pcs->pcs_number_of_primary_nvals
		{
			act_nodes = MeshElement->g_index[0];
			act_nodes_h = MeshElement->g_index[1];

			for (int i = 0; i < act_nodes_h; i++)
			{
				local_idx[i] = MeshElement->g_index[i + 2];
			}
		}
		else
		{
			act_nodes = nnodes;
			act_nodes_h = nnodesHQ;
			for (int i = 0; i < act_nodes_h; i++)
			{
				local_idx[i] = i;
			}
		}
	}
#endif
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   06/2004 WW Implementation
   Last modified:
**************************************************************************/
void CElement::CalculateRadius()
{
	Radius = 0.0;
	ComputeShapefct(1);
	for (int i = 0; i < nnodes; i++)
		Radius += shapefct[i] * X[i];
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   07/2005 WW Implementation
   Last modified:
**************************************************************************/
void CElement::setOrder(const int order)
{
	Order = order;
	if (Order == 1)
		nNodes = nnodes;
	else if (Order == 2)
		nNodes = nnodesHQ;
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   06/2004 WW Implementation
   02/2005 OK Case 1: line elements
   01/2010 NW Higher order line elements
   Last modified:
**************************************************************************/
void CElement::ConfigNumerics(MshElemType::type ele_type, const int nquadrature_points)
{
	assert(nquadrature_points > 0);
	// nGauss = GetNumericsGaussPoints(ElementType);
	switch (ele_type)
	{
		case MshElemType::LINE:
			ele_dim = 1;
			nGauss = 2;
			nGaussPoints = nGauss;
			ShapeFunction = ShapeFunctionLine;
			ShapeFunctionHQ = ShapeFunctionLineHQ;
			GradShapeFunction = GradShapeFunctionLine;
			GradShapeFunctionHQ = GradShapeFunctionLineHQ;
			extrapo_method = ExtrapolationMethod::EXTRAPO_LINEAR;
			return;
		case MshElemType::QUAD:
			ele_dim = 2;
			nGauss = nquadrature_points;
			nGaussPoints = nGauss * nGauss;
			ShapeFunction = ShapeFunctionQuad;
			ShapeFunctionHQ = ShapeFunctionQuadHQ;
			GradShapeFunction = GradShapeFunctionQuad;
			GradShapeFunctionHQ = GradShapeFunctionQuadHQ;
			extrapo_method = ExtrapolationMethod::EXTRAPO_LINEAR;
			return;
		case MshElemType::HEXAHEDRON:
			ele_dim = 3;
			nGauss = nquadrature_points;
			nGaussPoints = nGauss * nGauss * nGauss;
			ShapeFunction = ShapeFunctionHex;
			ShapeFunctionHQ = ShapeFunctionHexHQ;
			GradShapeFunction = GradShapeFunctionHex;
			GradShapeFunctionHQ = GradShapeFunctionHexHQ;
			extrapo_method = ExtrapolationMethod::EXTRAPO_LINEAR;
			return;
		case MshElemType::TRIANGLE:
			ele_dim = 2;
			nGaussPoints = nGauss = 3; // Fixed to 3
			ShapeFunction = ShapeFunctionTri;
			ShapeFunctionHQ = ShapeFunctionTriHQ;
			GradShapeFunction = GradShapeFunctionTri;
			GradShapeFunctionHQ = GradShapeFunctionTriHQ;
			extrapo_method = ExtrapolationMethod::EXTRAPO_LINEAR;
			return;
		case MshElemType::TETRAHEDRON:
			ele_dim = 3;
			//	   nGaussPoints = nGauss = 15;  // Fixed to 15
			nGaussPoints = nGauss = 5; // Fixed to 5
			ShapeFunction = ShapeFunctionTet;
			ShapeFunctionHQ = ShapeFunctionTetHQ;
			GradShapeFunction = GradShapeFunctionTet;
			GradShapeFunctionHQ = GradShapeFunctionTetHQ;
			extrapo_method = ExtrapolationMethod::EXTRAPO_LINEAR;
			return;
		case MshElemType::PRISM:
			ele_dim = 3;
			nGaussPoints = 6; // Fixed to 6
			nGauss = 3; // Fixed to 3
			ShapeFunction = ShapeFunctionPri;
			ShapeFunctionHQ = ShapeFunctionPriHQ;
			GradShapeFunction = GradShapeFunctionPri;
			GradShapeFunctionHQ = GradShapeFunctionPriHQ;
			extrapo_method = ExtrapolationMethod::EXTRAPO_AVERAGE;
			return;
		case MshElemType::PYRAMID:
			ele_dim = 3;
			if (Order == 1)
				nGaussPoints = nGauss = 5;
			else
				nGaussPoints = nGauss = 8; // 13;
			ShapeFunction = ShapeFunctionPyra;
			ShapeFunctionHQ = ShapeFunctionPyraHQ13;
			GradShapeFunction = GradShapeFunctionPyra;
			GradShapeFunctionHQ = GradShapeFunctionPyraHQ13;
			extrapo_method = ExtrapolationMethod::EXTRAPO_AVERAGE;
			return;
		case MshElemType::INVALID:
			std::cerr << "[CElement::ConfigNumerics] invalid element type"
			          << "\n";
			break;
		default:
			std::cerr << "[CElement::ConfigNumerics] unknown element type"
			          << "\n";
	}
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   06/2004 WW Implementation
   Last modified:
**************************************************************************/
double CElement::interpolate(double const* const nodalVal, const int order) const
{
	int nn = nnodes;
	double* inTerpo = shapefct;
	if (order == 2)
	{
		nn = nnodes;
		inTerpo = shapefctHQ;
	}
	double val = 0.0;
	for (int i = 0; i < nn; i++)
		val += nodalVal[i] * inTerpo[i];
	return val;
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   09/2005 WW Implementation
   Last modified:
**************************************************************************/
double CElement::interpolate(const int idx, CRFProcess* m_pcs, const int order)
{
	int i;
	int nn = nnodes;
	double* inTerpo = shapefct;
	double val = 0.0;
	if (order == 2)
	{
		nn = nnodes;
		inTerpo = shapefctHQ;
	}
	//
	for (i = 0; i < nn; i++)
		node_val[i] = m_pcs->GetNodeValue(nodes[i], idx);
	for (int i = 0; i < nn; i++)
		val += node_val[i] * inTerpo[i];
	return val;
}
/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   09/2005 WW Implementation
   Last modified:
**************************************************************************/
// double CElement::elemnt_average (const int idx, const int order)
double CElement::elemnt_average(const int idx, CRFProcess* m_pcs, const int order)
{
	int i;
	int nn = nnodes;
	double val = 0.0;
	// WW    double* inTerpo = shapefct;
	if (order == 2)
		nn = nnodes;
	// WW       inTerpo = shapefctHQ;
	//
	for (i = 0; i < nn; i++)
		node_val[i] = m_pcs->GetNodeValue(nodes[i], idx);
	return val / (double)nn;
}

/**************************************************************************
   The generalized Jacobian caculation

   Arguments:
    const double *unit:           Local coordiantes
   return
        The determinate of Jacobian
   Programmaenderungen:
   06/2006     WW
   02/2005 OK case 1, line elements
   01/2006 WW Axisymmtry
   09/2006 WW 1D element in 3D
**************************************************************************/
double CElement::computeJacobian(const int order)
{
	int k = 0;
	int nodes_number = nnodes;
	double DetJac = 0.0;
	double* dN = dshapefct;
	//    double *sh = shapefct;
	double dx, dy, dz;
	dx = dy = dz = 0.0;

	if (order == 2) // OK4104
	{
		nodes_number = nnodesHQ;
		dN = dshapefctHQ;
		GradShapeFunctionHQ(dN, unit);
	}
	else
		GradShapeFunction(dN, unit);
	for (size_t i = 0; i < ele_dim * ele_dim; i++)
		Jacobian[i] = 0.0;
	//--------------------------------------------------------------------
	switch (ele_dim)
	{
		//................................................................
		case 1:
			// If Line in X or Z direction, coordinate is saved in local X
			// If Line in 3D space, a transform is applied and cast coordinate in local X
			dx = X[1] - X[0]; //+Y[1]-Y[0];
			Jacobian[0] = 0.5 * dx;
			invJacobian[0] = 2.0 / dx;
			DetJac = Jacobian[0];
			// WW
			// if(MeshElement->area>0)
			DetJac *= MeshElement->area;
			// WW          DetJac*=MeshElement->GetFluxArea();//CMCD
			if (axisymmetry)
			{
				CalculateRadius();
				DetJac *= Radius; // 2.0*pai*Radius;
			}
			break;
		//................................................................
		case 2:
			for (int i = 0, j = nodes_number; i < nodes_number; i++, j++)
			{
				Jacobian[0] += X[i] * dN[i];
				Jacobian[1] += Y[i] * dN[i];
				Jacobian[2] += X[i] * dN[j];
				Jacobian[3] += Y[i] * dN[j];
			}

			DetJac = Jacobian[0] * Jacobian[3] - Jacobian[1] * Jacobian[2];
			if (fabs(DetJac) < MKleinsteZahl)
			{
				std::cout << "\n*** Jacobian: Det == 0 " << DetJac << "\n";
				abort();
			}
			invJacobian[0] = Jacobian[3];
			invJacobian[1] = -Jacobian[1];
			invJacobian[2] = -Jacobian[2];
			invJacobian[3] = Jacobian[0];
			for (size_t i = 0; i < ele_dim * ele_dim; i++)
				invJacobian[i] /= DetJac;
			//
			// By WW
			// if(MeshElement->area>0)
			DetJac *= MeshElement->area;
			// WW          DetJac*=MeshElement->GetFluxArea();//CMCD
			if (axisymmetry)
			{
				CalculateRadius();
				DetJac *= Radius; // 2.0*pai*Radius;
			}
			break;
		//................................................................
		case 3:
		{
			int j;
			for (int i = 0; i < nodes_number; i++)
			{
				j = i + nodes_number;
				k = i + 2 * nodes_number;

				Jacobian[0] += X[i] * dN[i];
				Jacobian[1] += Y[i] * dN[i];
				Jacobian[2] += Z[i] * dN[i];

				Jacobian[3] += X[i] * dN[j];
				Jacobian[4] += Y[i] * dN[j];
				Jacobian[5] += Z[i] * dN[j];

				Jacobian[6] += X[i] * dN[k];
				Jacobian[7] += Y[i] * dN[k];
				Jacobian[8] += Z[i] * dN[k];
			}
			DetJac = Jacobian[0] * (Jacobian[4] * Jacobian[8] - Jacobian[7] * Jacobian[5])
			         + Jacobian[6] * (Jacobian[1] * Jacobian[5] - Jacobian[4] * Jacobian[2])
			         + Jacobian[3] * (Jacobian[2] * Jacobian[7] - Jacobian[8] * Jacobian[1]);

			if (fabs(DetJac) < MKleinsteZahl)
			{
				std::cout << "\n*** Jacobian: DetJac == 0 " << DetJac << "\n";
				abort();
			}
			invJacobian[0] = Jacobian[4] * Jacobian[8] - Jacobian[7] * Jacobian[5];
			invJacobian[1] = Jacobian[2] * Jacobian[7] - Jacobian[1] * Jacobian[8];
			invJacobian[2] = Jacobian[1] * Jacobian[5] - Jacobian[2] * Jacobian[4];
			//
			invJacobian[3] = Jacobian[5] * Jacobian[6] - Jacobian[8] * Jacobian[3];
			invJacobian[4] = Jacobian[0] * Jacobian[8] - Jacobian[6] * Jacobian[2];
			invJacobian[5] = Jacobian[2] * Jacobian[3] - Jacobian[5] * Jacobian[0];
			//
			invJacobian[6] = Jacobian[3] * Jacobian[7] - Jacobian[6] * Jacobian[4];
			invJacobian[7] = Jacobian[1] * Jacobian[6] - Jacobian[7] * Jacobian[0];
			invJacobian[8] = Jacobian[0] * Jacobian[4] - Jacobian[3] * Jacobian[1];
			for (size_t i = 0; i < ele_dim * ele_dim; i++)
				invJacobian[i] /= DetJac;
			break;
		} // end case 3
	}
	//--------------------------------------------------------------------
	// Use absolute value (for grids by gmsh, whose orientation is clockwise)
	return fabs(DetJac);
}
/***************************************************************************
   GeoSys - Funktion: CElement::RealCoordinates

   Aufgabe:
        Mapping to real coordaintes from the local ones of quadratic traingle
   element.
   Formalparameter:
           E:
             double * x         : Array of size 3, real coordiantes
             const double *u    : Array of size 2, unit coordiantes

   Programming:
   06/2003     WW        Erste Version
   07/2005     WW        Change due to geometry element object
 **************************************************************************/
void CElement::RealCoordinates(double* realXYZ)
{
	int i;
	double* df = shapefct;
	if (Order == 2)
		df = shapefctHQ;
	for (i = 0; i < 3; i++)
		realXYZ[i] = 0.0;

	for (i = 0; i < nNodes; i++)
	{
		realXYZ[0] += df[i] * X[i];
		realXYZ[1] += df[i] * Y[i];
		realXYZ[2] += df[i] * Z[i];
	}
}
/***************************************************************************
   GeoSys - Funktion: CElement::UnitCoordinates

   Aufgabe:
        Get unit coodinates from the real ones
   element.
   Formalparameter:
           E:
             double * x         : Array of size 3, real coordiantes

   Programming:
   06/2003     WW        Erste Version
 **************************************************************************/
void CElement::UnitCoordinates(double* realXYZ)
{
	setOrder(Order);

	x1buff[0] = X[0];
	x1buff[1] = Y[0];
	x1buff[2] = Z[0];

	for (int i = 1; i < nNodes; i++)
	{
		x1buff[0] += X[i];
		x1buff[1] += Y[i];
		x1buff[2] += Z[i];
	}
	for (size_t i = 0; i < 3; i++)
		x1buff[i] /= (double)nNodes;

	for (size_t i = 0; i < ele_dim; i++)
		realXYZ[i] -= x1buff[i];

	for (size_t i = 0; i < ele_dim; i++)
	{
		unit[i] = 0.0;
		for (size_t j = 0; j < ele_dim; j++)
			unit[i] += invJacobian[j * ele_dim + i] * realXYZ[j];
	}

	for (size_t i = 0; i < ele_dim; i++)
		realXYZ[i] = unit[i];
}
/***************************************************************************

   08/2005     WW        Prism element
 **************************************************************************/
void CElement::SetGaussPoint(const int gp, int& gp_r, int& gp_s, int& gp_t)
{
	switch (MeshElement->GetElementType())
	{
		case MshElemType::LINE: // Line
			gp_r = gp;
			unit[0] = MXPGaussPkt(nGauss, gp_r);
			return;
		case MshElemType::QUAD: // Quadralateral
			gp_r = (int)(gp / nGauss);
			gp_s = gp % nGauss;
			unit[0] = MXPGaussPkt(nGauss, gp_r);
			unit[1] = MXPGaussPkt(nGauss, gp_s);
			return;
		case MshElemType::HEXAHEDRON: // Hexahedra
			gp_r = (int)(gp / (nGauss * nGauss));
			gp_s = (gp % (nGauss * nGauss));
			gp_t = gp_s % nGauss;
			gp_s /= nGauss;
			unit[0] = MXPGaussPkt(nGauss, gp_r);
			unit[1] = MXPGaussPkt(nGauss, gp_s);
			unit[2] = MXPGaussPkt(nGauss, gp_t);
			return;
		case MshElemType::TRIANGLE: // Triangle
			SamplePointTriHQ(gp, unit);
			break;
		case MshElemType::TETRAHEDRON: // Tedrahedra
			// To be flexible          SamplePointTet15(gp, unit);
			SamplePointTet5(gp, unit);
			return;
		case MshElemType::PRISM: // Prism
			gp_r = gp % nGauss;
			SamplePointTriHQ(gp_r, unit);
			//
			gp_s = nGaussPoints / nGauss;
			gp_t = (int)(gp / nGauss);
			unit[2] = MXPGaussPkt(gp_s, gp_t);
			return;
		case MshElemType::PYRAMID: // Pyramid
			if (Order == 1)
				SamplePointPyramid5(gp, unit);
			else
				SamplePointPyramid8(gp, unit); // SamplePointPyramid13(gp, unit);
			return;
		default:
			std::cerr << "CElement::SetGaussPoint invalid mesh element type given"
			          << "\n";
	}
}
/***************************************************************************
   GeoSys - Funktion:
           CElement:: GetGaussData(const int gp)

   Aufgabe:
          Get Gauss points and weights, compute Jacobian
   Formalparameter:
           E:
             const int gp   : Gauss point index

   Programming:
   06/2004     WW        Erste Version
   08/2005     WW        Prism element
   02/2005 OK case 1
   02/2007 WW Abstract the calcultion of Gauss point in one function
 **************************************************************************/
double CElement::GetGaussData(int gp, int& gp_r, int& gp_s, int& gp_t)
{
	double fkt = 0.0;
	SetGaussPoint(gp, gp_r, gp_s, gp_t);
	switch (MeshElement->GetElementType())
	{
		case MshElemType::LINE: // Line
			fkt = computeJacobian(Order) * MXPGaussFkt(nGauss, gp_r);
			break;
		case MshElemType::QUAD: // Quadralateral
			fkt = computeJacobian(Order);
			fkt *= MXPGaussFkt(nGauss, gp_r) * MXPGaussFkt(nGauss, gp_s);
			break;
		case MshElemType::HEXAHEDRON: // Hexahedra
			fkt = computeJacobian(Order);
			fkt *= MXPGaussFkt(nGauss, gp_r) * MXPGaussFkt(nGauss, gp_s) * MXPGaussFkt(nGauss, gp_t);
			break;
		case MshElemType::TRIANGLE: // Triangle
			fkt = computeJacobian(Order);
			fkt *= unit[2]; // Weights
			break;
		case MshElemType::TETRAHEDRON: // Tedrahedra
			// To be flexible          SamplePointTet15(gp, unit);
			fkt = computeJacobian(Order);
			fkt *= unit[3]; // Weights
			break;
		case MshElemType::PRISM: // Prism
			fkt = computeJacobian(Order);
			// Weights
			fkt *= MXPGaussFktTri(nGauss, gp_r) * MXPGaussFkt(gp_s, gp_t);
			break;
		case MshElemType::PYRAMID: // Pyramid
			fkt = computeJacobian(Order);
			fkt *= unit[3]; // Weights
			break;
		default:
			std::cerr << "CElement::GetGaussData invalid mesh element type given"
			          << "\n";
	}
	return fkt;
}

/***************************************************************************
   GeoSys - Funktion: FaceIntegration(const double *NodeVal)
   Task:   Used to treat Nuemann type boundary conditions (3D)
   Augument
       double *NodeVal : input, values of boundary conditions at all face node
                         Output, integration results of all face nodes
   Programming:
   06/2004     WW        Erste Version
 **************************************************************************/
void CElement::FaceIntegration(double* NodeVal)
{
	int i, gp, gp_r, gp_s;
	double fkt = 0.0, det, val;
	double* sf = shapefct;

	setOrder(Order);
	if (Order == 2)
	{
		sf = shapefctHQ;
		if (MeshElement->GetElementType() == MshElemType::QUAD)
			ShapeFunctionHQ = ShapeFunctionQuadHQ8;
	}

	det = MeshElement->GetVolume();
	for (i = 0; i < nNodes; i++)
		dbuff[i] = 0.0;
	// Loop over Gauss points
	for (gp = 0; gp < nGaussPoints; gp++)
	{
		//---------------------------------------------------------
		//  Get local coordinates and weights
		//  Compute Jacobian matrix and its determinate
		//---------------------------------------------------------
		switch (MeshElement->GetElementType())
		{
			case MshElemType::LINE: // Line
				gp_r = gp;
				unit[0] = MXPGaussPkt(nGauss, gp_r);
				fkt = 0.5 * det * MXPGaussFkt(nGauss, gp_r);
				break;
			case MshElemType::TRIANGLE: // Triangle
				SamplePointTriHQ(gp, unit);
				fkt = 2.0 * det * unit[2]; // Weights
				break;
			case MshElemType::QUAD: // Quadralateral
				gp_r = (int)(gp / nGauss);
				gp_s = gp % nGauss;
				unit[0] = MXPGaussPkt(nGauss, gp_r);
				unit[1] = MXPGaussPkt(nGauss, gp_s);
				fkt = 0.25 * det * MXPGaussFkt(nGauss, gp_r) * MXPGaussFkt(nGauss, gp_s);
				break;
			default:
				std::cerr << "CElement::FaceIntegration element type not handled"
				          << "\n";
		}

		ComputeShapefct(Order);
		val = 0.0;
		// Interpolation of value at Gauss point
		for (i = 0; i < nNodes; i++)
			val += NodeVal[i] * sf[i];
		// Integration
		for (i = 0; i < nNodes; i++)
			dbuff[i] += val * sf[i] * fkt;
	}
	for (i = 0; i < nNodes; i++)
		NodeVal[i] = dbuff[i];
}

/***************************************************************************
   GeoSys - Funktion:
           CElement::ComputeShapefct(const double *unit, const int order)

   Aufgabe:
         Compute values of shape function at integral point unit.
   Formalparameter:
           E:
             const double *u    : Array of size 2, unit coordiantes
             const int order    : 1, linear
                               2, quadratic

   Programming:
   06/2004     WW        Erste Version
 **************************************************************************/
void CElement::ComputeShapefct(const int order)
{
	if (order == 1)
		ShapeFunction(shapefct, unit);
	else if (order == 2)
		ShapeFunctionHQ(shapefctHQ, unit);
}

/***************************************************************************
   GeoSys - Funktion:
           CElement::ComputeGradShapefct(const double *unit, const int order)

   Aufgabe:
         Compute values of shape function at integral point unit.
   Formalparameter:
           E:
             const double *unit    : Array of size 2, unit coordiantes
             const int order    : 1, linear
                               2, quadratic

   Programming:
   06/2004     WW        Erste Version
   10/2005     WW        2D element transform in 3D space
   06/2007     WW        1D in 2D
 **************************************************************************/
void CElement::ComputeGradShapefct(int order)
{
	int j_times_ele_dim_plus_k, j_times_nNodes_plus_i;
	static double Var[3];
	double* dN = dshapefct;

	if (order == 2)
		dN = dshapefctHQ;

	setOrder(order);
	for (int i = 0; i < nNodes; i++)
	{
		size_t j(0);
		for (j = 0, j_times_nNodes_plus_i = i; j < ele_dim; j++, j_times_nNodes_plus_i += nNodes)
		{
			Var[j] = dN[j_times_nNodes_plus_i];
			dN[j_times_nNodes_plus_i] = 0.0;
		}
		for (j = 0, j_times_ele_dim_plus_k = 0, j_times_nNodes_plus_i = i; j < ele_dim;
		     j++, j_times_nNodes_plus_i += nNodes)
		{
			for (size_t k = 0; k < ele_dim; k++, j_times_ele_dim_plus_k++)
			{
				dN[j_times_nNodes_plus_i] += invJacobian[j_times_ele_dim_plus_k] * Var[k];
			}
		}
	}
	// 1D element in 3D
	if ((dim == 3 && ele_dim == 1) || (dim == 2 && ele_dim == 1))
		for (int i = 0; i < nNodes; i++)
		{
			for (size_t j = 1; j < dim; j++)
				dN[j * nNodes + i] = (*MeshElement->transform_tensor)(j)*dN[i];
			dN[i] *= (*MeshElement->transform_tensor)(0);
		}
	// 2D element in 3D
	if (dim == 3 && ele_dim == 2)
	{
		const size_t n_nodes_times_ele_dim(nNodes * ele_dim);
		for (size_t i = 0; i < n_nodes_times_ele_dim; i++)
			dShapefct[i] = dN[i];
		for (int i = 0; i < nNodes; i++)
			for (size_t j = 0; j < dim; j++)
			{
				dN[j * nNodes + i] = 0.0;
				for (size_t k = 0; k < ele_dim; k++)
					dN[j * nNodes + i] += (*MeshElement->transform_tensor)(j, k) * dShapefct[k * nNodes + i];
			}
	}
}
/***************************************************************************
   Center of reference element
   Programming:
   09/2005     WW        Erste Version
 **************************************************************************/
void CElement::SetCenterGP()
{
	// Center of the reference element
	unit[0] = unit[1] = unit[2] = 0.0;
	if (MeshElement->GetElementType() == MshElemType::TRIANGLE)
		unit[0] = unit[1] = 1.0 / 3.0;
	else if (MeshElement->GetElementType() == MshElemType::TETRAHEDRON)
		unit[0] = unit[1] = unit[2] = 0.25;
}
/***************************************************************************
   GeoSys - Funktion:
           CFiniteElementVec::GetLocalIndex()
           For quadralateral and hexahedra element on the assumption that
           selected Gauss points form a quadralateral or hexahedra
   Aufgabe:
           Accumulate stress at each nodes
   Formalparameter:

   Programming:
   06/2004   WW
 **************************************************************************/
int CElement::GetLocalIndex(const int gp_r, const int gp_s, int gp_t)
{
	int LoIndex = -1;
	double r, s, t;

	//---------------------------------------------------------
	// Accumulate strains
	//---------------------------------------------------------
	switch (MeshElement->GetElementType())
	{
		case MshElemType::QUAD: // Quadralateral
			r = MXPGaussPkt(nGauss, gp_r);
			s = MXPGaussPkt(nGauss, gp_s);
			if (r > 0.0 && s > 0.0)
				LoIndex = 0;
			else if (r < 0.0 && s > 0.0)
				LoIndex = 1;
			else if (r < 0.0 && s < 0.0)
				LoIndex = 2;
			else if (r > 0.0 && s < 0.0)
				LoIndex = 3;
			else if (fabs(r) < MKleinsteZahl && s > 0.0)
				LoIndex = 4;
			else if (r < 0.0 && fabs(s) < MKleinsteZahl)
				LoIndex = 5;
			else if (fabs(r) < MKleinsteZahl && s < 0.0)
				LoIndex = 6;
			else if (r > 0.0 && fabs(s) < MKleinsteZahl)
				LoIndex = 7;
			else if (fabs(r) < MKleinsteZahl && fabs(s) < MKleinsteZahl)
				LoIndex = 8;
			break;
		case MshElemType::HEXAHEDRON: // Hexahedra
			r = MXPGaussPkt(nGauss, gp_r);
			s = MXPGaussPkt(nGauss, gp_s);
			t = MXPGaussPkt(nGauss, gp_t);

			if (t > 0.0)
			{
				if (r > 0.0 && s > 0.0)
					LoIndex = 0;
				else if (r < 0.0 && s > 0.0)
					LoIndex = 1;
				else if (r < 0.0 && s < 0.0)
					LoIndex = 2;
				else if (r > 0.0 && s < 0.0)
					LoIndex = 3;
				else if (fabs(r) < MKleinsteZahl && s > 0.0)
					LoIndex = 8;
				else if (r < 0.0 && fabs(s) < MKleinsteZahl)
					LoIndex = 9;
				else if (fabs(r) < MKleinsteZahl && s < 0.0)
					LoIndex = 10;
				else if (r > 0.0 && fabs(s) < MKleinsteZahl)
					LoIndex = 11;
				else if (fabs(r) < MKleinsteZahl && fabs(s) < MKleinsteZahl)
					return -1;
			}
			else if (fabs(t) < MKleinsteZahl)
			{
				if (fabs(r) < MKleinsteZahl || fabs(s) < MKleinsteZahl)
					return -1;
				if (r > 0.0 && s > 0.0)
					LoIndex = 16;
				else if (r < 0.0 && s > 0.0)
					LoIndex = 17;
				else if (r < 0.0 && s < 0.0)
					LoIndex = 18;
				else if (r > 0.0 && s < 0.0)
					LoIndex = 19;
			}
			if (t < 0.0)
			{
				if (r > 0.0 && s > 0.0)
					LoIndex = 4;
				else if (r < 0.0 && s > 0.0)
					LoIndex = 5;
				else if (r < 0.0 && s < 0.0)
					LoIndex = 6;
				else if (r > 0.0 && s < 0.0)
					LoIndex = 7;
				else if (fabs(r) < MKleinsteZahl && s > 0.0)
					LoIndex = 12;
				else if (r < 0.0 && fabs(s) < MKleinsteZahl)
					LoIndex = 13;
				else if (fabs(r) < MKleinsteZahl && s < 0.0)
					LoIndex = 14;
				else if (r > 0.0 && fabs(s) < MKleinsteZahl)
					LoIndex = 15;
				else if (fabs(r) < MKleinsteZahl && fabs(s) < MKleinsteZahl)
					return -1;
			}
			break;
		default:
			std::cerr << "CElement::GetLocalIndex invalid mesh element type given"
			          << "\n";
	}
	return LoIndex;
}
/***************************************************************************
   GeoSys - Funktion:
   Programming:
   02/2007   WW
 **************************************************************************/
void CElement::SetExtropoGaussPoints(const int i)
{
	int j = 0;
	MshElemType::type ElementType = MeshElement->GetElementType();
	//
	switch (ElementType)
	{
		case MshElemType::TRIANGLE: // Triangle
			// Compute values at verteces
			// Compute values at verteces
			switch (i)
			{
				case 0:
					unit[0] = -0.1666666666667;
					unit[1] = -0.1666666666667;
					break;
				case 1:
					unit[0] = 1.6666666666667;
					unit[1] = -0.1666666666667;
					break;
				case 2:
					unit[0] = -0.1666666666667;
					unit[1] = 1.6666666666667;
					break;
			}
			break;
		case MshElemType::QUAD: // Quadralateral element
			// Extropolation over nodes
			switch (i)
			{
				case 0:
					unit[0] = Xi_p;
					unit[1] = Xi_p;
					break;
				case 1:
					unit[0] = -Xi_p;
					unit[1] = Xi_p;
					break;
				case 2:
					unit[0] = -Xi_p;
					unit[1] = -Xi_p;
					break;
				case 3:
					unit[0] = Xi_p;
					unit[1] = -Xi_p;
					break;
			}
			break;
		case MshElemType::HEXAHEDRON: // Hexahedra
			if (i < 4)
			{
				j = i;
				unit[2] = Xi_p;
			}
			else
			{
				j = i - 4;
				unit[2] = -Xi_p;
			}
			switch (j)
			{
				case 0:
					unit[0] = Xi_p;
					unit[1] = Xi_p;
					break;
				case 1:
					unit[0] = -Xi_p;
					unit[1] = Xi_p;
					break;
				case 2:
					unit[0] = -Xi_p;
					unit[1] = -Xi_p;
					break;
				case 3:
					unit[0] = Xi_p;
					unit[1] = -Xi_p;
					break;
			}
			break;
		case MshElemType::TETRAHEDRON: // Tedrahedra
			// Compute values at verteces
			switch (i)
			{
				case 0:
					unit[0] = -0.166666666666667;
					unit[1] = -0.166666666666667;
					unit[2] = -0.166666666666667;
					break;
				case 1:
					unit[0] = 1.5;
					unit[1] = -0.166666666666667;
					unit[2] = -0.166666666666667;
					break;
				case 2:
					unit[0] = -0.166666666666667;
					unit[1] = 1.5;
					unit[2] = -0.166666666666667;
					break;
				case 3:
					unit[0] = -0.166666666666667;
					unit[1] = -0.166666666666667;
					unit[2] = 1.5;
			}
			break;
		case MshElemType::LINE:
			break;
		case MshElemType::PYRAMID: // WW. 09.2012. WW
			SamplePointPyramid5(i, unit);
			break;
		default:
			unit[0] = unit[1] = unit[2] = 0.; // 07.01.2011. WW
			break;
	}
}

/***************************************************************************
   GeoSys - Funktion:
   Programming:
   05/2011   NW
 **************************************************************************/
double CElement::CalcXi_p()
{
	double Xi_p = 0.0;
	MshElemType::type ElementType = MeshElement->GetElementType();
	if (ElementType == MshElemType::QUAD || ElementType == MshElemType::HEXAHEDRON)
	{
		double r = .0;
		for (gp = 0; gp < nGauss; gp++)
		{
			r = MXPGaussPkt(nGauss, gp);
			if (fabs(r) > Xi_p)
				Xi_p = fabs(r);
		}
		r = 1.0 / Xi_p;
		Xi_p = r;
	}

	return Xi_p;
}

/***************************************************************************
   GeoSys - Funktion:
   Programming:
   05/2011   NW Implementation
 **************************************************************************/
double CElement::CalcAverageGaussPointValues(double* GpValues)
{
	// average
	double avg = .0;
	for (int j = 0; j < nGauss; j++)
		avg += GpValues[j];
	avg /= nGauss;

	return avg;
}

/**************************************************************************
   ElementMatrix::AllocateMemory

   Arguments:
    const int EleIndex:  Element index,
    int type          : type
                         used to get element type.
                         type = 0, for Possion type
                         type = 1, for Possion equation with deformation coupling
                         type = 2, for Navier equation
                         type = 3, for Navier equation with pressure coupling
   type = 4, Monlithic scheme of u-p coupling
   type = 5, Mass Transport
   default = 0.

   Programmaenderungen:
   01/2005     WW

**************************************************************************/
void ElementMatrix::AllocateMemory(CElem* ele, int type)
{
	int nnodes, nnodesHQ, dim, size;
	size = 0;
	// The following two lines will be updated when new FEMGEO is ready
	nnodes = ele->GetVertexNumber();
	nnodesHQ = ele->GetNodesNumber_H();
	dim = ele->GetDimension();
	switch (type)
	{
		case 0: // H || T Process
			Mass = new Matrix(nnodes, nnodes);
			//        Laplace = new SymMatrix(nnodes);
			Laplace = new Matrix(nnodes, nnodes);
			RHS = new Vec(nnodes);
			break;
		case 1: // HM Partioned scheme, Flow
			Mass = new Matrix(nnodes, nnodes);
			//        Laplace = new SymMatrix(nnodes);
			Laplace = new Matrix(nnodes, nnodes);
			RHS = new Vec(nnodes);
			CouplingB = new Matrix(nnodes, dim * nnodesHQ);
			break;
		case 2: // M_Process only
			size = dim * nnodesHQ;
			Stiffness = new Matrix(size, size);
			RHS = new Vec(size);
			break;
		case 3: // MH Partioned scheme, M_Process
			size = dim * nnodesHQ;
			Stiffness = new Matrix(size, size);
			RHS = new Vec(size);
			CouplingA = new Matrix(dim * nnodesHQ, nnodes);
			break;
		case 4: // HM monothlic scheme
			Mass = new Matrix(nnodes, nnodes);
			//        Laplace = new SymMatrix(nnodes);
			Laplace = new Matrix(nnodes, nnodes);
			size = dim * nnodesHQ;
			Stiffness = new Matrix(size, size);
			RHS = new Vec(size + nnodes);
			CouplingA = new Matrix(dim * nnodesHQ, nnodes);
			CouplingB = new Matrix(nnodes, dim * nnodesHQ);
			break;
		case 5: // Mass Transport process
			Mass = new Matrix(nnodes, nnodes);
			Laplace = new Matrix(nnodes, nnodes);
			Advection = new Matrix(nnodes, nnodes);
			Storage = new Matrix(nnodes, nnodes);
			Content = new Matrix(nnodes, nnodes);
			RHS = new Vec(nnodes);
			break;
	}
}

/**************************************************************************
   ElementMatrix::ElementMatrix

   Arguments:
      const int EleIndex:  Element index,
                           used to get element type.
   Programmaenderungen:
   01/2005     WW

**************************************************************************/
ElementMatrix::~ElementMatrix()
{
	if (Mass)
		delete Mass;
	if (Laplace)
		delete Laplace;
	if (Advection)
		delete Advection;
	if (Storage)
		delete Storage;
	if (Content)
		delete Content;
	if (RHS)
		delete RHS;
	if (CouplingA)
		delete CouplingA;
	if (CouplingB)
		delete CouplingB;
	Mass = NULL;
	Laplace = NULL;
	Advection = NULL;
	Storage = NULL;
	Content = NULL;
	RHS = NULL;
	CouplingA = NULL;
	CouplingB = NULL;
}

/**************************************************************************
CElement::FaceNormalFluxIntegration

Used for TOTAL_FLUX calculation

Programming:
11/2014   JOD

**************************************************************************/

void CElement::FaceNormalFluxIntegration(long /*element_index*/, double* NodeVal, double* NodeVal_adv,
                                         int* /*nodesFace*/, CElem* /*face*/, CRFProcess* m_pcs, double* normal_vector)
{
	int gp, gp_r, gp_s;
	double fkt = 0.0, det;
	double* sf = shapefct;
	double normal_diff_flux_interpol, normal_adv_flux_interpol;
	double dbuff_adv[10], flux[3];
	// ElementValue* gp_ele = ele_gp_value[element_index];

	setOrder(Order);
	if (Order == 2)
	{
		sf = shapefctHQ;
		if (MeshElement->GetElementType() == MshElemType::QUAD)
			ShapeFunctionHQ = ShapeFunctionQuadHQ8;
	}

	for (int i = 0; i < nNodes; i++)
	{
		dbuff[i] = 0.0;
		dbuff_adv[i] = 0.0;
	}

	det = MeshElement->GetVolume();
	// Loop over Gauss points
	for (gp = 0; gp < nGaussPoints; gp++)
	{
		//---------------------------------------------------------
		//  Get local coordinates and weights
		//  Compute Jacobian matrix and its determinate
		//---------------------------------------------------------
		switch (MeshElement->GetElementType())
		{
			case MshElemType::LINE: // Line
				gp_r = gp;
				unit[0] = MXPGaussPkt(nGauss, gp_r);
				fkt = 0.5 * det * MXPGaussFkt(nGauss, gp_r);
				break;
			case MshElemType::TRIANGLE: // Triangle
				SamplePointTriHQ(gp, unit);
				fkt = 2.0 * det * unit[2]; // Weights
				break;
			case MshElemType::QUAD: // Quadrilateral
				gp_r = (int)(gp / nGauss);
				gp_s = gp % nGauss;
				unit[0] = MXPGaussPkt(nGauss, gp_r);
				unit[1] = MXPGaussPkt(nGauss, gp_s);
				fkt = 0.25 * det * MXPGaussFkt(nGauss, gp_r) * MXPGaussFkt(nGauss, gp_s);
				break;
			default:
				std::cerr << "Error in mass balance calculation: CElement::FaceIntegration element type not supported"
				          << "\n";
		}
		//---------------------------------------------------------
		ComputeShapefct(Order);

		normal_diff_flux_interpol = 0.0;

		if ((m_pcs->getProcessType() == FiniteElement::GROUNDWATER_FLOW)
		    || (m_pcs->getProcessType() == FiniteElement::LIQUID_FLOW))
		{
			for (int i = 0; i < nNodes; i++) // Darcy flux
				normal_diff_flux_interpol += NodeVal[i] * sf[i];

			for (int i = 0; i < nNodes; i++) // Integration
				dbuff[i] += normal_diff_flux_interpol * sf[i] * fkt;
		}
		else if ((m_pcs->getProcessType() == FiniteElement::HEAT_TRANSPORT)
		         || (m_pcs->getProcessType() == FiniteElement::MASS_TRANSPORT))
		{
			normal_adv_flux_interpol = 0.0;

			for (int i = 0; i < nNodes; i++)
				normal_adv_flux_interpol += NodeVal_adv[i] * sf[i];

			for (int i = 0; i < nNodes; i++)
			{ // Integration
#ifdef USE_TRANSPORT_FLUX
				// Fick or Fourier diffusion
				for (int l = 0; l < 3; l++)
					flux[l] = gp_ele->TransportFlux(l, gp);
#endif
				normal_diff_flux_interpol
				    = PointProduction(flux, normal_vector); //    fabs(PointProduction(flux, normal_vector));
				dbuff[i] += normal_diff_flux_interpol * sf[i] * fkt;
				// advection
				dbuff_adv[i] += normal_adv_flux_interpol * sf[i] * fkt;
			}
		} // end transport
	} // end gauss points

	for (int i = 0; i < nNodes; i++)
	{
		NodeVal[i] = dbuff[i];
		NodeVal_adv[i] = dbuff_adv[i];
	}
}

} // end namespace FiniteElement
