#include "add_barycenter.h"

#include "verbose.h"
#include <algorithm>
#include <igl/barycenter.h>

template <typename Scalar, typename Index>
IGL_INLINE void igl::add_barycenter(
    const Eigen::PlainObjectBase<Scalar> & V, 
    const Eigen::PlainObjectBase<Index> & F, 
    Eigen::PlainObjectBase<Scalar> & VD, 
    Eigen::PlainObjectBase<Index> & FD)
{
  using namespace Eigen;
	// Compute face barycenter
	Eigen::MatrixXd BC;
	igl::barycenter(V,F,BC);

	// Add the barycenters to the vertices
	VD.resize(V.rows()+F.rows(),3);
	VD.block(0,0,V.rows(),3) = V;
	VD.block(V.rows(),0,F.rows(),3) = BC;

	// Each face is split four ways
	FD.resize(F.rows()*3,3);

	for (unsigned i=0; i<F.rows(); ++i)
	{
		int i0 = F(i,0);
		int i1 = F(i,1);
		int i2 = F(i,2);
		int i3 = V.rows() + i;

		Vector3i F0,F1,F2;
		F0 << i0,i1,i3;
		F1 << i1,i2,i3;
		F2 << i2,i0,i3;

		FD.row(i*3 + 0) = F0;
		FD.row(i*3 + 1) = F1;
		FD.row(i*3 + 2) = F2;
	}


}


#ifndef IGL_HEADER_ONLY
// Explicit template specialization
#endif