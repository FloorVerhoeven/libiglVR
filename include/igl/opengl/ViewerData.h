// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2014 Daniele Panozzo <daniele.panozzo@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef IGL_VIEWERDATA_H
#define IGL_VIEWERDATA_H

#include "../igl_inline.h"
#include "MeshGL.h"
#include <cassert>
#include <cstdint>
#include <Eigen/Core>
#include <memory>
#include <vector>
#include <mutex>

// Alec: This is a mesh class containing a variety of data types (normals,
// overlays, material colors, etc.)
//
namespace igl
{

// TODO: write documentation
namespace opengl
{

class ViewerData
{
public:
  ViewerData();

  ViewerData& operator = (const ViewerData& other) {
	  overlay_lock = std::unique_lock<std::mutex>(mu_overlay, std::defer_lock);
	  base_data_lock = std::unique_lock<std::mutex>(mu_base, std::defer_lock);

	  std::lock(mu, other.mu, *overlay_lock.mutex(), *other.overlay_lock.mutex(), *base_data_lock.mutex(), *other.base_data_lock.mutex());
	  std::lock_guard<std::mutex> self_lock(mu, std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock(other.mu, std::adopt_lock);
	  std::lock_guard<std::mutex> self_lock1(*overlay_lock.mutex(), std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock1(*other.overlay_lock.mutex(), std::adopt_lock);
	  std::lock_guard<std::mutex> self_lock2(*base_data_lock.mutex(), std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock2(*other.base_data_lock.mutex(), std::adopt_lock);

	  V = other.V;
	  F = other.F;
	  F_normals = other.F_normals;
	  V_normals = other.V_normals;

	  F_material_ambient = other.F_material_ambient;
	  F_material_diffuse = other.F_material_diffuse;
	  F_material_specular = other.F_material_specular;

	  V_material_ambient = other.V_material_ambient;
	  V_material_diffuse = other.V_material_diffuse;
	  V_material_specular = other.V_material_specular;


	  V_uv = other.V_uv;
	  F_uv = other.F_uv;

	  lines = other.lines;
	  points = other.points;
	  labels_positions = other.labels_positions;
	  labels_strings = other.labels_strings;

	  stroke_points = other.stroke_points;
	  laser_points = other.laser_points;
	  hand_point = other.hand_point;

	  texture_R = other.texture_R;
	  texture_G = other.texture_G;
	  texture_B = other.texture_B; 
	  texture_A = other.texture_A;

	  dirty = other.dirty;
	  face_based = other.face_based;

	  show_overlay = other.show_overlay;
	  show_overlay_depth = other.show_overlay_depth;
	  show_texture = other.show_texture;
	  show_strokes = other.show_strokes;
	  show_laser = other.show_laser;
	  show_faces = other.show_faces;
	  show_lines = other.show_lines;
	  show_vertid = other.show_vertid;
	  show_faceid = other.show_faceid;
	  invert_normals = other.invert_normals;

	  point_size = other.point_size;
	  line_width = other.line_width;
	  overlay_line_width = other.overlay_line_width;
	  stroke_line_width = other.stroke_line_width;
	  laser_line_width = other.laser_line_width;
	  line_color = other.line_color;

	  shininess = other.shininess;

	  id = other.id;


	  /*overlay_lock.unlock();
	  other.overlay_lock.unlock();
	  base_data_lock.unlock();
	  other.base_data_lock.unlock();*/
  }

  ViewerData(const ViewerData& other) {
	  overlay_lock = std::unique_lock<std::mutex>(mu_overlay, std::defer_lock);
	  base_data_lock = std::unique_lock<std::mutex>(mu_base, std::defer_lock);

	  std::lock(mu, other.mu, *overlay_lock.mutex(), *other.overlay_lock.mutex(), *base_data_lock.mutex(), *other.base_data_lock.mutex());
	  std::lock_guard<std::mutex> self_lock(mu, std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock(other.mu, std::adopt_lock);
	  std::lock_guard<std::mutex> self_lock1(*overlay_lock.mutex(), std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock1(*other.overlay_lock.mutex(), std::adopt_lock);
	  std::lock_guard<std::mutex> self_lock2(*base_data_lock.mutex(), std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock2(*other.base_data_lock.mutex(), std::adopt_lock);

	  V = other.V;
	  F = other.F;
	  F_normals = other.F_normals;
	  V_normals = other.V_normals;

	  F_material_ambient = other.F_material_ambient;
	  F_material_diffuse = other.F_material_diffuse;
	  F_material_specular = other.F_material_specular;

	  V_material_ambient = other.V_material_ambient;
	  V_material_diffuse = other.V_material_diffuse;
	  V_material_specular = other.V_material_specular;


	  V_uv = other.V_uv;
	  F_uv = other.F_uv;

	  lines = other.lines;
	  points = other.points;
	  labels_positions = other.labels_positions;
	  labels_strings = other.labels_strings;

	  stroke_points = other.stroke_points;
	  laser_points = other.laser_points;
	  hand_point = other.hand_point;

	  texture_R = other.texture_R;
	  texture_G = other.texture_G;
	  texture_B = other.texture_B;
	  texture_A = other.texture_A;

	  dirty = other.dirty;
	  face_based = other.face_based;

	  show_overlay = other.show_overlay;
	  show_overlay_depth = other.show_overlay_depth;
	  show_texture = other.show_texture;
	  show_strokes = other.show_strokes;
	  show_laser = other.show_laser;
	  show_faces = other.show_faces;
	  show_lines = other.show_lines;
	  show_vertid = other.show_vertid;
	  show_faceid = other.show_faceid;
	  invert_normals = other.invert_normals;

	  point_size = other.point_size;
	  line_width = other.line_width;
	  overlay_line_width = other.overlay_line_width;
	  stroke_line_width = other.stroke_line_width;
	  laser_line_width = other.laser_line_width;
	  line_color = other.line_color;

	  shininess = other.shininess;

	  id = other.id;


	  /*overlay_lock.unlock();
	  other.overlay_lock.unlock();
	  base_data_lock.unlock();
	  other.base_data_lock.unlock();
	  */
  }

  // Empty all fields
  IGL_INLINE void clear();

  // Change the visualization mode, invalidating the cache if necessary
  IGL_INLINE void set_face_based(bool newvalue);

  // Helpers that can draw the most common meshes
  IGL_INLINE void set_mesh(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F);
  IGL_INLINE void set_vertices(const Eigen::MatrixXd& V);
  IGL_INLINE void set_normals(const Eigen::MatrixXd& N);

  // Set the color of the mesh
  //
  // Inputs:
  //   C  #V|#F|1 by 3 list of colors
  IGL_INLINE void set_colors(const Eigen::MatrixXd &C);
  // Set per-vertex UV coordinates
  //
  // Inputs:
  //   UV  #V by 2 list of UV coordinates (indexed by F)
  IGL_INLINE void set_uv(const Eigen::MatrixXd& UV);
  // Set per-corner UV coordinates
  //
  // Inputs:
  //   UV_V  #UV by 2 list of UV coordinates
  //   UV_F  #F by 3 list of UV indices into UV_V
  IGL_INLINE void set_uv(const Eigen::MatrixXd& UV_V, const Eigen::MatrixXi& UV_F);
  // Set the texture associated with the mesh.
  //
  // Inputs:
  //   R  width by height image matrix of red channel
  //   G  width by height image matrix of green channel
  //   B  width by height image matrix of blue channel
  //
  IGL_INLINE void set_texture(
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& R,
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& G,
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& B);

  // Set the texture associated with the mesh.
  //
  // Inputs:
  //   R  width by height image matrix of red channel
  //   G  width by height image matrix of green channel
  //   B  width by height image matrix of blue channel
  //   A  width by height image matrix of alpha channel
  //
  IGL_INLINE void set_texture(
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& R,
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& G,
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& B,
    const Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& A);

  // Sets points given a list of point vertices. In constrast to `set_points`
  // this will (purposefully) clober existing points.
  //
  // Inputs:
  //   P  #P by 3 list of vertex positions
  //   C  #P|1 by 3 color(s)
  IGL_INLINE void set_points(
    const Eigen::MatrixXd& P,
    const Eigen::MatrixXd& C);
  IGL_INLINE void add_points(const Eigen::MatrixXd& P,  const Eigen::MatrixXd& C);

  //Sets linestrip points given a list of strokepoints
  // Inputs:
  // SP #SP by 3 (or 2) list of vertex positions
  IGL_INLINE void set_stroke_points(const Eigen::MatrixXd& SP);
  IGL_INLINE void add_stroke_points(const Eigen::MatrixXd& SP);

  //Sets linestrip points given a list of 2 laser points (start and end)
  // Inputs:
  // LP #LP by 3 (or 2) list of vertex positions
  IGL_INLINE void set_laser_points(const Eigen::MatrixXd& LP);
  IGL_INLINE void add_laser_points(const Eigen::MatrixXd& LP);

  // Sets hand point given a list of point vertices. In constrast to `add_points`
  // this will (purposefully) clober existing points.
  //
  // Inputs:
  //   P  #P by 3 list of vertex positions
  //   C  #P|1 by 3 color(s)
  IGL_INLINE void set_hand_point(
	  const Eigen::MatrixXd& P,
	  const Eigen::MatrixXd& C);
  IGL_INLINE void add_hand_point(const Eigen::MatrixXd& P, const Eigen::MatrixXd& C);

  // Sets edges given a list of edge vertices and edge indices. In constrast
  // to `add_edges` this will (purposefully) clober existing edges.
  //
  // Inputs:
  //   P  #P by 3 list of vertex positions
  //   E  #E by 2 list of edge indices into P
  //   C  #E|1 by 3 color(s)
  IGL_INLINE void set_edges (const Eigen::MatrixXd& P, const Eigen::MatrixXi& E, const Eigen::MatrixXd& C);
  // Alec: This is very confusing. Why does add_edges have a different API from
  // set_edges? 
  IGL_INLINE void add_edges (const Eigen::MatrixXd& P1, const Eigen::MatrixXd& P2, const Eigen::MatrixXd& C);
  IGL_INLINE void add_label (const Eigen::VectorXd& P,  const std::string& str);

  // Computes the normals of the mesh
  IGL_INLINE void compute_normals();

  // Assigns uniform colors to all faces/vertices
  IGL_INLINE void uniform_colors(
    const Eigen::Vector3d& diffuse,
    const Eigen::Vector3d& ambient,
    const Eigen::Vector3d& specular);

  // Assigns uniform colors to all faces/vertices
  IGL_INLINE void uniform_colors(
    const Eigen::Vector4d& ambient,
    const Eigen::Vector4d& diffuse,
    const Eigen::Vector4d& specular);

  // Generates a default grid texture
  IGL_INLINE void grid_texture();


  mutable std::mutex mu;
  mutable std::mutex mu_overlay;
  mutable std::mutex mu_base;
  mutable std::unique_lock<std::mutex> overlay_lock;
  mutable std::unique_lock<std::mutex> base_data_lock;

  Eigen::MatrixXd V; // Vertices of the current mesh (#V x 3)
  Eigen::MatrixXi F; // Faces of the mesh (#F x 3)

  // Per face attributes
  Eigen::MatrixXd F_normals; // One normal per face

  Eigen::MatrixXd F_material_ambient; // Per face ambient color
  Eigen::MatrixXd F_material_diffuse; // Per face diffuse color
  Eigen::MatrixXd F_material_specular; // Per face specular color

  // Per vertex attributes
  Eigen::MatrixXd V_normals; // One normal per vertex

  Eigen::MatrixXd V_material_ambient; // Per vertex ambient color
  Eigen::MatrixXd V_material_diffuse; // Per vertex diffuse color
  Eigen::MatrixXd V_material_specular; // Per vertex specular color

  // UV parametrization
  Eigen::MatrixXd V_uv; // UV vertices
  Eigen::MatrixXi F_uv; // optional faces for UVs

  // Texture
  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> texture_R;
  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> texture_G;
  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> texture_B;
  Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic> texture_A;

  // Overlays

  // Lines plotted over the scene
  // (Every row contains 9 doubles in the following format S_x, S_y, S_z, T_x, T_y, T_z, C_r, C_g, C_b),
  // with S and T the coordinates of the two vertices of the line in global coordinates, and C the color in floating point rgb format
  Eigen::MatrixXd lines;

  // Points plotted over the scene
  // (Every row contains 6 doubles in the following format P_x, P_y, P_z, C_r, C_g, C_b),
  // with P the position in global coordinates of the center of the point, and C the color in floating point rgb format
  Eigen::MatrixXd points;

  // Points belonging to curves that are drawn on the scene
  // Every row contains 3 doubles in the following format: P_x, P_y, P_z, with P the position in global coordinates of the point
  Eigen::MatrixXd stroke_points;

  //Start and end points belonging to laser rays that are drawn on the scene
  Eigen::MatrixXd laser_points;
  Eigen::MatrixXd hand_point;

  // Text labels plotted over the scene
  // Textp contains, in the i-th row, the position in global coordinates where the i-th label should be anchored
  // Texts contains in the i-th position the text of the i-th label
  Eigen::MatrixXd           labels_positions;
  std::vector<std::string>  labels_strings;

  // Marks dirty buffers that need to be uploaded to OpenGL
  uint32_t dirty;

  // Enable per-face or per-vertex properties
  bool face_based;

  // Visualization options
  bool show_overlay;
  bool show_overlay_depth;
  bool show_texture;
  bool show_strokes;
  bool show_laser;
  bool show_faces;
  bool show_lines;
  bool show_vertid;
  bool show_faceid;
  bool invert_normals;

  // Point size / line width
  float point_size;
  float line_width;
  float overlay_line_width;
  float stroke_line_width;
  float laser_line_width;
  Eigen::Vector4f line_color;

  // Shape material
  float shininess;

  // Unique identifier
  int id;

  // OpenGL representation of the mesh
  igl::opengl::MeshGL meshgl;

  // Update contents from a 'Data' instance
  IGL_INLINE void updateGL(
    const igl::opengl::ViewerData& data,
    const bool invert_normals,
    igl::opengl::MeshGL& meshgl);
};

} // namespace opengl
} // namespace igl

////////////////////////////////////////////////////////////////////////////////

#include <igl/serialize.h>
namespace igl
{
  namespace serialization
  {
    inline void serialization(bool s, igl::opengl::ViewerData& obj, std::vector<char>& buffer)
    {
      SERIALIZE_MEMBER(V);
      SERIALIZE_MEMBER(F);
      SERIALIZE_MEMBER(F_normals);
      SERIALIZE_MEMBER(F_material_ambient);
      SERIALIZE_MEMBER(F_material_diffuse);
      SERIALIZE_MEMBER(F_material_specular);
      SERIALIZE_MEMBER(V_normals);
      SERIALIZE_MEMBER(V_material_ambient);
      SERIALIZE_MEMBER(V_material_diffuse);
      SERIALIZE_MEMBER(V_material_specular);
      SERIALIZE_MEMBER(V_uv);
      SERIALIZE_MEMBER(F_uv);
      SERIALIZE_MEMBER(texture_R);
      SERIALIZE_MEMBER(texture_G);
      SERIALIZE_MEMBER(texture_B);
      SERIALIZE_MEMBER(texture_A);
      SERIALIZE_MEMBER(lines);
      SERIALIZE_MEMBER(points);
      SERIALIZE_MEMBER(labels_positions);
      SERIALIZE_MEMBER(labels_strings);
	  SERIALIZE_MEMBER(stroke_points);
	  SERIALIZE_MEMBER(laser_points);
	  SERIALIZE_MEMBER(hand_point);
      SERIALIZE_MEMBER(dirty);
      SERIALIZE_MEMBER(face_based);
      SERIALIZE_MEMBER(show_faces);
      SERIALIZE_MEMBER(show_lines);
	  SERIALIZE_MEMBER(show_strokes);
	  SERIALIZE_MEMBER(show_laser);
      SERIALIZE_MEMBER(invert_normals);
      SERIALIZE_MEMBER(show_overlay);
      SERIALIZE_MEMBER(show_overlay_depth);
      SERIALIZE_MEMBER(show_vertid);
      SERIALIZE_MEMBER(show_faceid);
      SERIALIZE_MEMBER(show_texture);
      SERIALIZE_MEMBER(point_size);
      SERIALIZE_MEMBER(line_width);
	  SERIALIZE_MEMBER(overlay_line_width);
	  SERIALIZE_MEMBER(stroke_line_width);
	  SERIALIZE_MEMBER(laser_line_width);
      SERIALIZE_MEMBER(line_color);
      SERIALIZE_MEMBER(shininess);
      SERIALIZE_MEMBER(id);
//	  SERIALIZE_MEMBER(mu);
	//  SERIALIZE_MEMBER(overlay_lock);
	  //SERIALIZE_MEMBER(base_data_lock);
    }
    template<>
    inline void serialize(const igl::opengl::ViewerData& obj, std::vector<char>& buffer)
    {
      serialization(true, const_cast<igl::opengl::ViewerData&>(obj), buffer);
    }
    template<>
    inline void deserialize(igl::opengl::ViewerData& obj, const std::vector<char>& buffer)
    {
      serialization(false, obj, const_cast<std::vector<char>&>(buffer));
      obj.dirty = igl::opengl::MeshGL::DIRTY_ALL;
    }
  }
}

#ifndef IGL_STATIC_LIBRARY
#  include "ViewerData.cpp"
#endif

#endif
