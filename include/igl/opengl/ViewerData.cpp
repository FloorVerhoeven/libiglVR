// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2014 Daniele Panozzo <daniele.panozzo@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.

#include "ViewerData.h"

#include "../per_face_normals.h"
#include "../material_colors.h"
#include "../parula.h"
#include "../per_vertex_normals.h"
#include "../quat_to_mat.h"

#include <iostream>


IGL_INLINE igl::opengl::ViewerData::ViewerData()
	: dirty(MeshGL::DIRTY_ALL),
	show_faces(true),
	show_lines(true),
	invert_normals(false),
	show_overlay(true),
	show_overlay_depth(true),
	show_vertid(false),
	show_faceid(false),
	show_texture(false),
	show_laser(true),
	point_size(15),
	line_width(0.5f),
	overlay_line_width(1000.6f),
	laser_line_width(1.6f),
	linestrip_line_width(2.0f),
	line_color(0, 0, 0, 1),
	shininess(35.0f),
	volumetric_diffuse(CYAN_DIFFUSE[0], CYAN_DIFFUSE[1], CYAN_DIFFUSE[2]), // (1.0f, 0.5f, 0.125f),
	volumetric_ambient(CYAN_AMBIENT[0], CYAN_AMBIENT[1], CYAN_AMBIENT[2]),//0.125f, 0.125f, 0.0f),
	volumetric_specular(CYAN_SPECULAR[0], CYAN_SPECULAR[1], CYAN_SPECULAR[2]),// 0.5f, 0.5f, 0.5f),
	mesh_trackball_angle(Eigen::Quaternionf::Identity()),
	mesh_translation(Eigen::Vector3f::Zero()),
	mesh_model_translation(Eigen::Matrix4f::Identity()),
	id(-1)
{
	clear();
};

IGL_INLINE void igl::opengl::ViewerData::set_face_based(bool newvalue)
{
	std::lock_guard<std::mutex> lock(mu);

	if (face_based != newvalue)
	{
		face_based = newvalue;
		dirty = MeshGL::DIRTY_ALL;
	}
}

IGL_INLINE void igl::opengl::ViewerData::set_mesh_translation() {
	std::unique_lock<std::mutex> lck(mu_translations);
	mesh_translation = -V.colwise().mean().eval().cast<float>();
}

IGL_INLINE void igl::opengl::ViewerData::set_mesh_model_translation() {
	std::unique_lock<std::mutex> lck(mu_translations);
	mesh_model_translation.col(3).head(3) = -mesh_translation;
}

// Helpers that draws the most common meshes
IGL_INLINE void igl::opengl::ViewerData::set_mesh(
	const Eigen::MatrixXd& _V, const Eigen::MatrixXi& _F)
{
	using namespace std;
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	Eigen::MatrixXd V_temp;

	// If V only has two columns, pad with a column of zeros
	if (_V.cols() == 2)
	{
		V_temp = Eigen::MatrixXd::Zero(_V.rows(), 3);
		V_temp.block(0, 0, _V.rows(), 2) = _V;
	}
	else
		V_temp = _V;

	if (V.rows() == 0 && F.rows() == 0)
	{
		V = V_temp;
		F = _F;

		//See answer 3 on https://stackoverflow.com/questions/2415082/when-to-use-recursive-mutex for an alternative, non-recursive mutex implementation
		compute_normals();
		uniform_colors(
			Eigen::Vector3d(GOLD_AMBIENT[0], GOLD_AMBIENT[1], GOLD_AMBIENT[2]),
			Eigen::Vector3d(GOLD_DIFFUSE[0], GOLD_DIFFUSE[1], GOLD_DIFFUSE[2]),
			Eigen::Vector3d(GOLD_SPECULAR[0], GOLD_SPECULAR[1], GOLD_SPECULAR[2]));

		grid_texture();
	}
	else
	{
		if (_V.rows() == V.rows() && _F.rows() == F.rows())
		{
			V = V_temp;
			F = _F;
		}
		else
			cerr << "ERROR (set_mesh): The new mesh has a different number of vertices/faces. Please clear the mesh before plotting." << endl;
	}
	dirty |= MeshGL::DIRTY_FACE | MeshGL::DIRTY_POSITION;
}

IGL_INLINE void igl::opengl::ViewerData::set_vertices(const Eigen::MatrixXd& _V)
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	V = _V;
	assert(F.size() == 0 || F.maxCoeff() < V.rows());
	dirty |= MeshGL::DIRTY_POSITION;
}

IGL_INLINE void igl::opengl::ViewerData::set_normals(const Eigen::MatrixXd& N)
{
	using namespace std;
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	if (N.rows() == V.rows())
	{
		set_face_based(false);
		V_normals = N;
	}
	else if (N.rows() == F.rows() || N.rows() == F.rows() * 3)
	{
		set_face_based(true);
		F_normals = N;
	}
	else
		cerr << "ERROR (set_normals): Please provide a normal per face, per corner or per vertex." << endl;
	dirty |= MeshGL::DIRTY_NORMAL;
}

IGL_INLINE void igl::opengl::ViewerData::set_colors(const Eigen::MatrixXd &C)
{
	using namespace std;
	using namespace Eigen;
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	if (C.rows() > 0 && C.cols() == 1)
	{
		Eigen::MatrixXd C3;
		igl::parula(C, true, C3);
		return set_colors(C3);
	}
	// Ambient color should be darker color
	const auto ambient = [](const MatrixXd & C)->MatrixXd
	{
		MatrixXd T = 0.1*C;
		T.col(3) = C.col(3);
		return T;
	};
	// Specular color should be a less saturated and darker color: dampened
	// highlights
	const auto specular = [](const MatrixXd & C)->MatrixXd
	{
		const double grey = 0.3;
		MatrixXd T = grey + 0.1*(C.array() - grey);
		T.col(3) = C.col(3);
		return T;
	};
	if (C.rows() == 1)
	{
		for (unsigned i = 0; i < V_material_diffuse.rows(); ++i)
		{
			if (C.cols() == 3)
				V_material_diffuse.row(i) << C.row(0), 1;
			else if (C.cols() == 4)
				V_material_diffuse.row(i) << C.row(0);
		}
		V_material_ambient = ambient(V_material_diffuse);
		V_material_specular = specular(V_material_diffuse);

		for (unsigned i = 0; i < F_material_diffuse.rows(); ++i)
		{
			if (C.cols() == 3)
				F_material_diffuse.row(i) << C.row(0), 1;
			else if (C.cols() == 4)
				F_material_diffuse.row(i) << C.row(0);
		}
		F_material_ambient = ambient(F_material_diffuse);
		F_material_specular = specular(F_material_diffuse);
	}
	else if (C.rows() == V.rows())
	{
		set_face_based(false);
		for (unsigned i = 0; i < V_material_diffuse.rows(); ++i)
		{
			if (C.cols() == 3)
				V_material_diffuse.row(i) << C.row(i), 1;
			else if (C.cols() == 4)
				V_material_diffuse.row(i) << C.row(i);
		}
		V_material_ambient = ambient(V_material_diffuse);
		V_material_specular = specular(V_material_diffuse);
	}
	else if (C.rows() == F.rows())
	{
		set_face_based(true);
		for (unsigned i = 0; i < F_material_diffuse.rows(); ++i)
		{
			if (C.cols() == 3)
				F_material_diffuse.row(i) << C.row(i), 1;
			else if (C.cols() == 4)
				F_material_diffuse.row(i) << C.row(i);
		}
		F_material_ambient = ambient(F_material_diffuse);
		F_material_specular = specular(F_material_diffuse);
	}
	else
		cerr << "ERROR (set_colors): Please provide a single color, or a color per face or per vertex." << endl;
	dirty |= MeshGL::DIRTY_DIFFUSE;
}

IGL_INLINE void igl::opengl::ViewerData::set_uv(const Eigen::MatrixXd& UV)
{
	using namespace std;
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	if (UV.rows() == V.rows())
	{
		set_face_based(false);
		V_uv = UV;
	}
	else
		cerr << "ERROR (set_UV): Please provide uv per vertex." << endl;;
	dirty |= MeshGL::DIRTY_UV;
}

IGL_INLINE void igl::opengl::ViewerData::set_uv(const Eigen::MatrixXd& UV_V, const Eigen::MatrixXi& UV_F)
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	set_face_based(true);
	V_uv = UV_V.block(0, 0, UV_V.rows(), 2);
	F_uv = UV_F;
	dirty |= MeshGL::DIRTY_UV;
}

IGL_INLINE void igl::opengl::ViewerData::set_texture(
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& R,
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& G,
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& B)
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	texture_R = R;
	texture_G = G;
	texture_B = B;
	texture_A = Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>::Constant(R.rows(), R.cols(), 255);
	dirty |= MeshGL::DIRTY_TEXTURE;
}

IGL_INLINE void igl::opengl::ViewerData::set_texture(
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& R,
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& G,
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& B,
	const Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>& A)
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);

	texture_R = R;
	texture_G = G;
	texture_B = B;
	texture_A = A;
	dirty |= MeshGL::DIRTY_TEXTURE;
}

IGL_INLINE void igl::opengl::ViewerData::set_points(
	const Eigen::MatrixXd& P,
	const Eigen::MatrixXd& C)
{
	// clear existing points
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	points.resize(0, 0);
	add_points(P, C);
}

IGL_INLINE void igl::opengl::ViewerData::add_points(const Eigen::MatrixXd& P, const Eigen::MatrixXd& C)
{
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	Eigen::MatrixXd P_temp;

	// If P only has two columns, pad with a column of zeros
	if (P.cols() == 2)
	{
		P_temp = Eigen::MatrixXd::Zero(P.rows(), 3);
		P_temp.block(0, 0, P.rows(), 2) = P;
	}
	else
		P_temp = P;

	int lastid = points.rows();
	points.conservativeResize(points.rows() + P_temp.rows(), 6);
	for (unsigned i = 0; i < P_temp.rows(); ++i)
		points.row(lastid + i) << P_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1);
	dirty |= MeshGL::DIRTY_OVERLAY_POINTS;
}

IGL_INLINE void igl::opengl::ViewerData::set_laser_points(const Eigen::MatrixXd& LP, const Eigen::MatrixXd& C) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	laser_points.resize(0, 0);
	if (LP.rows() > 0) {
		add_laser_points(LP, C); //Will take care of unlocking
	}
	else {
		dirty |= MeshGL::DIRTY_LASER;
	}
}

IGL_INLINE void igl::opengl::ViewerData::add_laser_points(const Eigen::MatrixXd& LP, const Eigen::MatrixXd& C) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);
	Eigen::MatrixXd LP_temp;

	//If LP only has 2 columns, pad with a zero column
	if (LP.cols() == 2) {
		LP_temp = Eigen::MatrixXd::Zero(LP.rows(), 3);
		LP_temp.block(0, 0, LP.rows(), 2) = LP;
	}
	else {
		LP_temp = LP;
	}
	int lastid = laser_points.rows();
	laser_points.conservativeResize(laser_points.rows() + LP_temp.rows(), 6);
	for (unsigned i = 0; i < LP_temp.rows(); ++i) {
		laser_points.row(lastid + i) << LP_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1);
	}

	dirty |= MeshGL::DIRTY_LASER;
}

IGL_INLINE void igl::opengl::ViewerData::set_linestrip(const Eigen::MatrixXd& LP, const Eigen::MatrixXd& C) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	linestrip.resize(0, 0);
	if (LP.rows() > 0) {
		add_linestrip(LP, C); //Will take care of unlocking
	}
	else {
		dirty |= MeshGL::DIRTY_OVERLAY_STRIP;
	}
}

IGL_INLINE void igl::opengl::ViewerData::add_linestrip(const Eigen::MatrixXd& LP, const Eigen::MatrixXd& C) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);
	Eigen::MatrixXd LP_temp;

	//If LP only has 2 columns, pad with a zero column
	if (LP.cols() == 2) {
		LP_temp = Eigen::MatrixXd::Zero(LP.rows(), 3);
		LP_temp.block(0, 0, LP.rows(), 2) = LP;
	}
	else {
		LP_temp = LP;
	}
	int lastid = linestrip.rows();
	linestrip.conservativeResize(linestrip.rows() + LP_temp.rows(), 6);
	for (unsigned i = 0; i < LP_temp.rows(); ++i) {
		linestrip.row(lastid + i) << LP_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1);
	}

	dirty |= MeshGL::DIRTY_OVERLAY_STRIP;
}

IGL_INLINE void igl::opengl::ViewerData::set_hand_point(
	const Eigen::MatrixXd& HP,
	const Eigen::MatrixXd& C)
{
	// clear existing points
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	hand_point.resize(0, 0);
	add_hand_point(HP, C); //Will take care of unlocking
}

IGL_INLINE void igl::opengl::ViewerData::add_hand_point(const Eigen::MatrixXd& HP, const Eigen::MatrixXd& C)
{
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);


	Eigen::MatrixXd HP_temp;

	// If P only has two columns, pad with a column of zeros
	if (HP.cols() == 2)
	{
		HP_temp = Eigen::MatrixXd::Zero(HP.rows(), 3);
		HP_temp.block(0, 0, HP.rows(), 2) = HP;
	}
	else
		HP_temp = HP;

	int lastid = hand_point.rows();
	hand_point.conservativeResize(hand_point.rows() + HP_temp.rows(), 6);
	for (unsigned i = 0; i < HP_temp.rows(); ++i)
		hand_point.row(lastid + i) << HP_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1);

	dirty |= MeshGL::DIRTY_HAND_POINT;
}

IGL_INLINE void igl::opengl::ViewerData::set_edges(
	const Eigen::MatrixXd& P,
	const Eigen::MatrixXi& E,
	const Eigen::MatrixXd& C)
{
	using namespace Eigen;
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	lines.resize(E.rows(), 9);
	assert(C.cols() == 3);
	for (int e = 0; e < E.rows(); e++)
	{
		RowVector3d color;
		if (C.size() == 3)
		{
			color << C;
		}
		else if (C.rows() == E.rows())
		{
			color << C.row(e);
		}
		lines.row(e) << P.row(E(e, 0)), P.row(E(e, 1)), color;
	}
	dirty |= MeshGL::DIRTY_OVERLAY_LINES;
}

IGL_INLINE void igl::opengl::ViewerData::add_edges(const Eigen::MatrixXd& P1, const Eigen::MatrixXd& P2, const Eigen::MatrixXd& C)
{
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	Eigen::MatrixXd P1_temp, P2_temp;

	// If P1 only has two columns, pad with a column of zeros
	if (P1.cols() == 2)
	{
		P1_temp = Eigen::MatrixXd::Zero(P1.rows(), 3);
		P1_temp.block(0, 0, P1.rows(), 2) = P1;
		P2_temp = Eigen::MatrixXd::Zero(P2.rows(), 3);
		P2_temp.block(0, 0, P2.rows(), 2) = P2;
	}
	else
	{
		P1_temp = P1;
		P2_temp = P2;
	}

	int lastid = lines.rows();
	lines.conservativeResize(lines.rows() + P1_temp.rows(), 9);
	for (unsigned i = 0; i < P1_temp.rows(); ++i)
		lines.row(lastid + i) << P1_temp.row(i), P2_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1);

	dirty |= MeshGL::DIRTY_OVERLAY_LINES;
}

/*IGL_INLINE void igl::opengl::ViewerData::set_volumetric_lines(const Eigen::MatrixXd& pos, const Eigen::VectorXd& lengths, const Eigen::MatrixXd& dir, const Eigen::MatrixXd& C) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	volumetric_lines.resize(0, 0);
	if (pos.rows() > 0) {
		add_volumetric_lines(pos, lengths, dir, C); //Will take care of unlocking
	}
	else {
		dirty |= MeshGL::DIRTY_VOLUMETRIC_LINES;
	}
}

IGL_INLINE void igl::opengl::ViewerData::add_volumetric_lines(const Eigen::MatrixXd& pos, const Eigen::VectorXd& lengths, const Eigen::MatrixXd& dir, const Eigen::MatrixXd& C) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);
	Eigen::MatrixXd pos_temp, dir_temp;

	//If LP only has 2 columns, pad with a zero column
	if (pos.cols() == 2) {
		pos_temp = Eigen::MatrixXd::Zero(pos.rows(), 3);
		pos_temp.block(0, 0, pos.rows(), 2) = pos;
		dir_temp = Eigen::MatrixXd::Zero(dir.rows(), 3);
		dir_temp.block(0, 0, dir.rows(), 2) = dir;
	}
	else {
		pos_temp = pos;
		dir_temp = dir;
	}

	int lastid = volumetric_lines.rows();
	volumetric_lines.conservativeResize(volumetric_lines.rows() + pos_temp.rows(), 10);
	for (unsigned i = 0; i < pos_temp.rows(); ++i)
		volumetric_lines.row(lastid + i) << pos_temp.row(i), lengths[i], dir_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1);

	dirty |= MeshGL::DIRTY_VOLUMETRIC_LINES;
}*/

IGL_INLINE void igl::opengl::ViewerData::set_volumetric_lines(const Eigen::MatrixXd& LP, const Eigen::MatrixXd& C, const Eigen::MatrixXd& N) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	volumetric_lines.resize(0, 0);
	if (LP.rows() > 0) {
		add_volumetric_lines(LP, C, N); //Will take care of unlocking
	}
	else {
		dirty |= MeshGL::DIRTY_VOLUMETRIC_LINES;
	}
}

IGL_INLINE void igl::opengl::ViewerData::add_volumetric_lines(const Eigen::MatrixXd& LP, const Eigen::MatrixXd& C, const Eigen::MatrixXd& N) {
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);
	Eigen::MatrixXd LP_temp, N_temp;

	//If LP only has 2 columns, pad with a zero column
	if (LP.cols() == 2) {
		LP_temp = Eigen::MatrixXd::Zero(LP.rows(), 3);
		LP_temp.block(0, 0, LP.rows(), 2) = LP;
		N_temp = Eigen::MatrixXd::Zero(N.rows(), 3);
		N_temp.block(0, 0, N.rows(), 2) = N;
	}
	else {
		LP_temp = LP;
		N_temp = N;
	}	

	int lastid = volumetric_lines.rows();
	volumetric_lines.conservativeResize(volumetric_lines.rows() + LP_temp.rows(), 6);
	for (unsigned i = 0; i < LP_temp.rows(); ++i)
		//volumetric_lines.row(lastid + i) << LP_temp.row(i), i < C.rows() ? C.row(i) : C.row(C.rows() - 1), i < N_temp.rows() ? N_temp.row(i) : N_temp.row(N_temp.rows()-1);
		volumetric_lines.row(lastid + i) << LP_temp.row(i), i < N_temp.rows() ? N_temp.row(i) : N_temp.row(N_temp.rows() - 1);


	dirty |= MeshGL::DIRTY_VOLUMETRIC_LINES;
}

IGL_INLINE void igl::opengl::ViewerData::add_label(const Eigen::VectorXd& P, const std::string& str)
{
	std::unique_lock<std::recursive_mutex> lck(mu_overlay);

	Eigen::RowVectorXd P_temp;

	// If P only has two columns, pad with a column of zeros
	if (P.size() == 2)
	{
		P_temp = Eigen::RowVectorXd::Zero(3);
		P_temp << P.transpose(), 0;
	}
	else
		P_temp = P;

	int lastid = labels_positions.rows();
	labels_positions.conservativeResize(lastid + 1, 3);
	labels_positions.row(lastid) = P_temp;
	labels_strings.push_back(str);
}

IGL_INLINE void igl::opengl::ViewerData::clear()
{
	std::lock_guard<std::mutex> lock(mu);

	V = Eigen::MatrixXd(0, 3);
	F = Eigen::MatrixXi(0, 3);

	F_material_ambient = Eigen::MatrixXd(0, 4);
	F_material_diffuse = Eigen::MatrixXd(0, 4);
	F_material_specular = Eigen::MatrixXd(0, 4);

	V_material_ambient = Eigen::MatrixXd(0, 4);
	V_material_diffuse = Eigen::MatrixXd(0, 4);
	V_material_specular = Eigen::MatrixXd(0, 4);

	F_normals = Eigen::MatrixXd(0, 3);
	V_normals = Eigen::MatrixXd(0, 3);

	V_uv = Eigen::MatrixXd(0, 2);
	F_uv = Eigen::MatrixXi(0, 3);

	lines = Eigen::MatrixXd(0, 9);
	points = Eigen::MatrixXd(0, 6);
	labels_positions = Eigen::MatrixXd(0, 3);
	laser_points = Eigen::MatrixXd(0, 3);
	hand_point = Eigen::MatrixXd(0, 3);
	linestrip = Eigen::MatrixXd(0, 3);
	volumetric_lines = Eigen::MatrixXd(0, 6);
	mesh_trackball_angle = Eigen::Quaternionf::Identity();
	mesh_translation = Eigen::Vector3f::Zero();
	mesh_model_translation = Eigen::Matrix4f::Identity();
	labels_strings.clear();

	face_based = false;
}

IGL_INLINE void igl::opengl::ViewerData::compute_normals()
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);


	igl::per_face_normals(V, F, F_normals);
	igl::per_vertex_normals(V, F, F_normals, V_normals);
	dirty |= MeshGL::DIRTY_NORMAL;
}

IGL_INLINE void igl::opengl::ViewerData::uniform_colors(
	const Eigen::Vector3d& ambient,
	const Eigen::Vector3d& diffuse,
	const Eigen::Vector3d& specular)
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);


	Eigen::Vector4d ambient4;
	Eigen::Vector4d diffuse4;
	Eigen::Vector4d specular4;

	ambient4 << ambient, 1;
	diffuse4 << diffuse, 1;
	specular4 << specular, 1;

	uniform_colors(ambient4, diffuse4, specular4);

}

IGL_INLINE void igl::opengl::ViewerData::uniform_colors(
	const Eigen::Vector4d& ambient,
	const Eigen::Vector4d& diffuse,
	const Eigen::Vector4d& specular)
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);


	V_material_ambient.resize(V.rows(), 4);
	V_material_diffuse.resize(V.rows(), 4);
	V_material_specular.resize(V.rows(), 4);

	for (unsigned i = 0; i < V.rows(); ++i)
	{
		V_material_ambient.row(i) = ambient;
		V_material_diffuse.row(i) = diffuse;
		V_material_specular.row(i) = specular;
	}

	F_material_ambient.resize(F.rows(), 4);
	F_material_diffuse.resize(F.rows(), 4);
	F_material_specular.resize(F.rows(), 4);

	for (unsigned i = 0; i < F.rows(); ++i)
	{
		F_material_ambient.row(i) = ambient;
		F_material_diffuse.row(i) = diffuse;
		F_material_specular.row(i) = specular;
	}
	dirty |= MeshGL::DIRTY_SPECULAR | MeshGL::DIRTY_DIFFUSE | MeshGL::DIRTY_AMBIENT;
}

IGL_INLINE void igl::opengl::ViewerData::grid_texture()
{
	std::unique_lock<std::recursive_mutex> lck(mu_base);


	// Don't do anything for an empty mesh
	if (V.rows() == 0)
	{
		V_uv.resize(V.rows(), 2);
		return;
	}
	if (V_uv.rows() == 0)
	{
		V_uv = V.block(0, 0, V.rows(), 2);
		V_uv.col(0) = V_uv.col(0).array() - V_uv.col(0).minCoeff();
		V_uv.col(0) = V_uv.col(0).array() / V_uv.col(0).maxCoeff();
		V_uv.col(1) = V_uv.col(1).array() - V_uv.col(1).minCoeff();
		V_uv.col(1) = V_uv.col(1).array() / V_uv.col(1).maxCoeff();
		V_uv = V_uv.array() * 10;
		dirty |= MeshGL::DIRTY_TEXTURE;
	}

	unsigned size = 128;
	unsigned size2 = size / 2;
	texture_R.resize(size, size);
	for (unsigned i = 0; i < size; ++i)
	{
		for (unsigned j = 0; j < size; ++j)
		{
			texture_R(i, j) = 0;
			if ((i < size2 && j < size2) || (i >= size2 && j >= size2))
				texture_R(i, j) = 255;
		}
	}

	texture_G = texture_R;
	texture_B = texture_R;
	texture_A = Eigen::Matrix<unsigned char, Eigen::Dynamic, Eigen::Dynamic>::Constant(texture_R.rows(), texture_R.cols(), 255);
	dirty |= MeshGL::DIRTY_TEXTURE;
}

IGL_INLINE void igl::opengl::ViewerData::updateGL(
	const igl::opengl::ViewerData& data,
	const bool invert_normals,
	igl::opengl::MeshGL& meshgl
)
{
	if (!meshgl.is_initialized)
	{
		meshgl.init();
	}

	std::lock(mu_overlay, mu_base);
	std::lock_guard<std::recursive_mutex> lock1(mu_overlay, std::adopt_lock);
	std::lock_guard<std::recursive_mutex> lock2(mu_base, std::adopt_lock);

	bool per_corner_uv = (data.F_uv.rows() == data.F.rows());
	bool per_corner_normals = (data.F_normals.rows() == 3 * data.F.rows());

	meshgl.dirty |= data.dirty;

	// Input:
	//   X  #F by dim quantity
	// Output:
	//   X_vbo  #F*3 by dim scattering per corner
	const auto per_face = [&data](
		const Eigen::MatrixXd & X,
		MeshGL::RowMatrixXf & X_vbo)
	{
		assert(X.cols() == 4);
		X_vbo.resize(data.F.rows() * 3, 4);
		for (unsigned i = 0; i < data.F.rows(); ++i)
			for (unsigned j = 0; j < 3; ++j)
				X_vbo.row(i * 3 + j) = X.row(i).cast<float>();
	};

	// Input:
	//   X  #V by dim quantity
	// Output:
	//   X_vbo  #F*3 by dim scattering per corner
	const auto per_corner = [&data](
		const Eigen::MatrixXd & X,
		MeshGL::RowMatrixXf & X_vbo)
	{
		X_vbo.resize(data.F.rows() * 3, X.cols());
		for (unsigned i = 0; i < data.F.rows(); ++i)
			for (unsigned j = 0; j < 3; ++j)
				X_vbo.row(i * 3 + j) = X.row(data.F(i, j)).cast<float>();
	};

	if (!data.face_based)
	{
		if (!(per_corner_uv || per_corner_normals))
		{
			// Vertex positions
			if (meshgl.dirty & MeshGL::DIRTY_POSITION)
				meshgl.V_vbo = data.V.cast<float>();

			// Vertex normals
			if (meshgl.dirty & MeshGL::DIRTY_NORMAL)
			{
				meshgl.V_normals_vbo = data.V_normals.cast<float>();
				if (invert_normals)
					meshgl.V_normals_vbo = -meshgl.V_normals_vbo;
			}

			// Per-vertex material settings
			if (meshgl.dirty & MeshGL::DIRTY_AMBIENT)
				meshgl.V_ambient_vbo = data.V_material_ambient.cast<float>();
			if (meshgl.dirty & MeshGL::DIRTY_DIFFUSE)
				meshgl.V_diffuse_vbo = data.V_material_diffuse.cast<float>();
			if (meshgl.dirty & MeshGL::DIRTY_SPECULAR)
				meshgl.V_specular_vbo = data.V_material_specular.cast<float>();

			// Face indices
			if (meshgl.dirty & MeshGL::DIRTY_FACE)
				meshgl.F_vbo = data.F.cast<unsigned>();

			// Texture coordinates
			if (meshgl.dirty & MeshGL::DIRTY_UV)
			{
				meshgl.V_uv_vbo = data.V_uv.cast<float>();
			}
		}
		else
		{

			// Per vertex properties with per corner UVs
			if (meshgl.dirty & MeshGL::DIRTY_POSITION)
			{
				per_corner(data.V, meshgl.V_vbo);
			}

			if (meshgl.dirty & MeshGL::DIRTY_AMBIENT)
			{
				meshgl.V_ambient_vbo.resize(data.F.rows() * 3, 4);
				for (unsigned i = 0; i < data.F.rows(); ++i)
					for (unsigned j = 0; j < 3; ++j)
						meshgl.V_ambient_vbo.row(i * 3 + j) = data.V_material_ambient.row(data.F(i, j)).cast<float>();
			}
			if (meshgl.dirty & MeshGL::DIRTY_DIFFUSE)
			{
				meshgl.V_diffuse_vbo.resize(data.F.rows() * 3, 4);
				for (unsigned i = 0; i < data.F.rows(); ++i)
					for (unsigned j = 0; j < 3; ++j)
						meshgl.V_diffuse_vbo.row(i * 3 + j) = data.V_material_diffuse.row(data.F(i, j)).cast<float>();
			}
			if (meshgl.dirty & MeshGL::DIRTY_SPECULAR)
			{
				meshgl.V_specular_vbo.resize(data.F.rows() * 3, 4);
				for (unsigned i = 0; i < data.F.rows(); ++i)
					for (unsigned j = 0; j < 3; ++j)
						meshgl.V_specular_vbo.row(i * 3 + j) = data.V_material_specular.row(data.F(i, j)).cast<float>();
			}

			if (meshgl.dirty & MeshGL::DIRTY_NORMAL)
			{
				meshgl.V_normals_vbo.resize(data.F.rows() * 3, 3);
				for (unsigned i = 0; i < data.F.rows(); ++i)
					for (unsigned j = 0; j < 3; ++j)
						meshgl.V_normals_vbo.row(i * 3 + j) =
						per_corner_normals ?
						data.F_normals.row(i * 3 + j).cast<float>() :
						data.V_normals.row(data.F(i, j)).cast<float>();


				if (invert_normals)
					meshgl.V_normals_vbo = -meshgl.V_normals_vbo;
			}

			if (meshgl.dirty & MeshGL::DIRTY_FACE)
			{
				meshgl.F_vbo.resize(data.F.rows(), 3);
				for (unsigned i = 0; i < data.F.rows(); ++i)
					meshgl.F_vbo.row(i) << i * 3 + 0, i * 3 + 1, i * 3 + 2;
			}

			if (meshgl.dirty & MeshGL::DIRTY_UV)
			{
				meshgl.V_uv_vbo.resize(data.F.rows() * 3, 2);
				for (unsigned i = 0; i < data.F.rows(); ++i)
					for (unsigned j = 0; j < 3; ++j)
						meshgl.V_uv_vbo.row(i * 3 + j) =
						data.V_uv.row(per_corner_uv ?
							data.F_uv(i, j) : data.F(i, j)).cast<float>();
			}
		}
	}
	else
	{
		if (meshgl.dirty & MeshGL::DIRTY_POSITION)
		{
			per_corner(data.V, meshgl.V_vbo);
		}
		if (meshgl.dirty & MeshGL::DIRTY_AMBIENT)
		{
			per_face(data.F_material_ambient, meshgl.V_ambient_vbo);
		}
		if (meshgl.dirty & MeshGL::DIRTY_DIFFUSE)
		{
			per_face(data.F_material_diffuse, meshgl.V_diffuse_vbo);
		}
		if (meshgl.dirty & MeshGL::DIRTY_SPECULAR)
		{
			per_face(data.F_material_specular, meshgl.V_specular_vbo);
		}

		if (meshgl.dirty & MeshGL::DIRTY_NORMAL)
		{
			meshgl.V_normals_vbo.resize(data.F.rows() * 3, 3);
			for (unsigned i = 0; i < data.F.rows(); ++i)
				for (unsigned j = 0; j < 3; ++j)
					meshgl.V_normals_vbo.row(i * 3 + j) =
					per_corner_normals ?
					data.F_normals.row(i * 3 + j).cast<float>() :
					data.F_normals.row(i).cast<float>();

			if (invert_normals)
				meshgl.V_normals_vbo = -meshgl.V_normals_vbo;
		}

		if (meshgl.dirty & MeshGL::DIRTY_FACE)
		{
			meshgl.F_vbo.resize(data.F.rows(), 3);
			for (unsigned i = 0; i < data.F.rows(); ++i)
				meshgl.F_vbo.row(i) << i * 3 + 0, i * 3 + 1, i * 3 + 2;
		}

		if (meshgl.dirty & MeshGL::DIRTY_UV)
		{
			meshgl.V_uv_vbo.resize(data.F.rows() * 3, 2);
			for (unsigned i = 0; i < data.F.rows(); ++i)
				for (unsigned j = 0; j < 3; ++j)
					meshgl.V_uv_vbo.row(i * 3 + j) = data.V_uv.row(per_corner_uv ? data.F_uv(i, j) : data.F(i, j)).cast<float>();
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_TEXTURE)
	{
		meshgl.tex_u = data.texture_R.rows();
		meshgl.tex_v = data.texture_R.cols();
		meshgl.tex.resize(data.texture_R.size() * 4);
		for (unsigned i = 0; i < data.texture_R.size(); ++i)
		{
			meshgl.tex(i * 4 + 0) = data.texture_R(i);
			meshgl.tex(i * 4 + 1) = data.texture_G(i);
			meshgl.tex(i * 4 + 2) = data.texture_B(i);
			meshgl.tex(i * 4 + 3) = data.texture_A(i);
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_OVERLAY_LINES)
	{
		meshgl.lines_V_vbo.resize(data.lines.rows() * 2, 3);
		meshgl.lines_V_colors_vbo.resize(data.lines.rows() * 2, 3);
		meshgl.lines_F_vbo.resize(data.lines.rows() * 2, 1);
		for (unsigned i = 0; i < data.lines.rows(); ++i)
		{
			meshgl.lines_V_vbo.row(2 * i + 0) = data.lines.block<1, 3>(i, 0).cast<float>();
			meshgl.lines_V_vbo.row(2 * i + 1) = data.lines.block<1, 3>(i, 3).cast<float>();
			meshgl.lines_V_colors_vbo.row(2 * i + 0) = data.lines.block<1, 3>(i, 6).cast<float>();
			meshgl.lines_V_colors_vbo.row(2 * i + 1) = data.lines.block<1, 3>(i, 6).cast<float>();
			meshgl.lines_F_vbo(2 * i + 0) = 2 * i + 0;
			meshgl.lines_F_vbo(2 * i + 1) = 2 * i + 1;
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_OVERLAY_POINTS)
	{
		meshgl.points_V_vbo.resize(data.points.rows(), 3);
		meshgl.points_V_colors_vbo.resize(data.points.rows(), 3);
		meshgl.points_F_vbo.resize(data.points.rows(), 1);
		for (unsigned i = 0; i < data.points.rows(); ++i)
		{
			meshgl.points_V_vbo.row(i) = data.points.block<1, 3>(i, 0).cast<float>();
			meshgl.points_V_colors_vbo.row(i) = data.points.block<1, 3>(i, 3).cast<float>();
			meshgl.points_F_vbo(i) = i;
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_LASER) {
		meshgl.laser_points_V_vbo.resize(data.laser_points.rows(), 3);
		meshgl.laser_points_F_vbo.resize(data.laser_points.rows(), 1);
		meshgl.laser_V_colors_vbo.resize(data.laser_points.rows(), 3);
		for (unsigned i = 0; i < data.laser_points.rows(); ++i) {
			meshgl.laser_points_V_vbo.row(i) = data.laser_points.block<1, 3>(i, 0).transpose().cast<float>();
			meshgl.laser_points_F_vbo(i) = i;
			meshgl.laser_V_colors_vbo.row(i) = data.laser_points.block<1, 3>(i, 3).transpose().cast<float>();
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_HAND_POINT) {
		meshgl.hand_point_V_vbo.resize(data.hand_point.rows(), 3);
		meshgl.hand_point_V_colors_vbo.resize(data.hand_point.rows(), 3);
		meshgl.hand_point_F_vbo.resize(data.hand_point.rows(), 1);
		for (unsigned i = 0; i < data.hand_point.rows(); ++i) {
			meshgl.hand_point_V_vbo.row(i) = data.hand_point.block<1, 3>(i, 0).transpose().cast<float>();
			meshgl.hand_point_V_colors_vbo.row(i) = data.hand_point.block<1, 3>(i, 3).transpose().cast<float>();
			meshgl.hand_point_F_vbo(i) = i;
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_OVERLAY_STRIP) {
		meshgl.overlay_strip_V_vbo.resize(data.linestrip.rows(), 3);
		meshgl.overlay_strip_F_vbo.resize(data.linestrip.rows(), 1);
		meshgl.overlay_strip_V_colors_vbo.resize(data.linestrip.rows(), 3);
		for (unsigned i = 0; i < data.linestrip.rows(); ++i) {
			meshgl.overlay_strip_V_vbo.row(i) = data.linestrip.block<1, 3>(i, 0).transpose().cast<float>();
			meshgl.overlay_strip_F_vbo(i) = i;
			meshgl.overlay_strip_V_colors_vbo.row(i) = data.linestrip.block<1, 3>(i, 3).transpose().cast<float>();
		}
	}

	if (meshgl.dirty & MeshGL::DIRTY_VOLUMETRIC_LINES) {
		meshgl.volumetric_lines_V_vbo.resize(data.volumetric_lines.rows(), 6);
	//	meshgl.volumetric_lines_F_vbo.resize(data.volumetric_lines.rows() * 2, 1);
	//	meshgl.volumetric_lines_colors_vbo.resize(data.volumetric_lines.rows() * 2, 3);
	//	meshgl.volumetric_lines_normals_vbo.resize(data.volumetric_lines.rows() * 2, 3);
		for (unsigned i = 0; i < data.volumetric_lines.rows(); ++i) {
			meshgl.volumetric_lines_V_vbo.row(i) = data.volumetric_lines.block<1, 6>(i, 0).transpose().cast<float>();
		//	meshgl.volumetric_lines_V_vbo.row(2 * i + 1) = data.volumetric_lines.block<1, 3>(i, 3).transpose().cast<float>();
		//	meshgl.volumetric_lines_F_vbo(2 * i + 0) = 2 * i + 0;
		//	meshgl.volumetric_lines_F_vbo(2 * i + 1) = 2 * i + 1;
	//		meshgl.volumetric_lines_colors_vbo.row(2 * i + 0) = data.volumetric_lines.block<1, 3>(i, 6).transpose().cast<float>();
		//	meshgl.volumetric_lines_normals_vbo.row(2 * i +1) = data.volumetric_lines.block<1, 3>(i, 9).transpose().cast<float>();
		}
	}
}

IGL_INLINE void igl::opengl::ViewerData::rotate() { //Takes the trackball rotation as parameter to ensure it has been updated
	std::lock(mu_overlay, mu_base, mu_translations, mu_trackball_angle);
	std::lock_guard<std::recursive_mutex> lock1(mu_overlay, std::adopt_lock);
	std::lock_guard<std::recursive_mutex> lock2(mu_base, std::adopt_lock);
	std::lock_guard<std::mutex> lock3(mu_translations, std::adopt_lock);
	std::lock_guard<std::mutex> lock4(mu_trackball_angle, std::adopt_lock);

	float mat[16];
	igl::quat_to_mat((mesh_trackball_angle).coeffs().data(), mat);
	Eigen::Matrix4f rotation = Eigen::Matrix4f::Identity();
	for (unsigned i = 0; i < 4; ++i) {
		for (unsigned j = 0; j < 4; ++j) {
			rotation(i, j) = mat[i + 4 * j];
		}
	}
	rotation.col(3).head(3) += rotation.topLeftCorner(3, 3)*mesh_translation;
	Eigen::Matrix4f place_back = Eigen::Matrix4f::Identity();
	place_back.col(3).head(3) = -mesh_translation;
	Eigen::MatrixXd V_tmp(4, V.rows());
	V_tmp.block(0, 0, 3, V.rows()) = V.transpose();
	V_tmp.row(3) = Eigen::RowVectorXd::Constant(V.rows(), 1);
	V = ((place_back.cast<double>()*rotation.cast<double>()*V_tmp).topRows(3)).transpose();

	dirty |= MeshGL::DIRTY_ALL;
}

IGL_INLINE void igl::opengl::ViewerData::rotate_points(Eigen::MatrixXd& points_3D) {
	std::lock(mu_translations, mu_trackball_angle);
	std::lock_guard<std::mutex> lock1(mu_translations, std::adopt_lock);
	std::lock_guard<std::mutex> lock2(mu_trackball_angle, std::adopt_lock);

	float mat[16];
	igl::quat_to_mat((mesh_trackball_angle).coeffs().data(), mat);
	Eigen::Matrix4f rotation = Eigen::Matrix4f::Identity();
	for (unsigned i = 0; i < 4; ++i) {
		for (unsigned j = 0; j < 4; ++j) {
			rotation(i, j) = mat[i + 4 * j];
		}
	}
	rotation.col(3).head(3) += rotation.topLeftCorner(3, 3) * mesh_translation;
	Eigen::Matrix4f place_back = Eigen::Matrix4f::Identity();
	place_back.col(3).head(3) = -mesh_translation;

	Eigen::MatrixXd points_tmp(4, points_3D.rows());
	points_tmp.block(0, 0, 3, points_3D.rows()) = points_3D.transpose();
	points_tmp.row(3) = Eigen::RowVectorXd::Constant(points_3D.rows(), 1);
	points_3D = ((place_back.cast<double>()*rotation.cast<double>()*points_tmp).topRows(3)).transpose();
}