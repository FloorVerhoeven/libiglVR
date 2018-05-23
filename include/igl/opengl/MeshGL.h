// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2014 Daniele Panozzo <daniele.panozzo@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef IGL_OPENGL_MESHGL_H
#define IGL_OPENGL_MESHGL_H

// Coverts mesh data inside a igl::ViewerData class in an OpenGL
// compatible format The class includes a shader and the opengl calls to plot
// the data

#include <igl/igl_inline.h>
#include <Eigen/Core>

namespace igl
{
namespace opengl
{

class MeshGL
{
public:
  typedef unsigned int GLuint;

  enum DirtyFlags
  {
    DIRTY_NONE           = 0x0000,
    DIRTY_POSITION       = 0x0001,
    DIRTY_UV             = 0x0002,
    DIRTY_NORMAL         = 0x0004,
    DIRTY_AMBIENT        = 0x0008,
    DIRTY_DIFFUSE        = 0x0010,
    DIRTY_SPECULAR       = 0x0020,
    DIRTY_TEXTURE        = 0x0040,
    DIRTY_FACE           = 0x0080,
    DIRTY_MESH           = 0x00FF,
    DIRTY_OVERLAY_LINES  = 0x0100,
    DIRTY_OVERLAY_POINTS = 0x0200,
    DIRTY_ALL            = 0x03FF,
	DIRTY_STROKE		 = 0x0800,
	DIRTY_LASER			 = 0x1000,
	DIRTY_HAND_POINT	 = 0x2000,
	DIRTY_AVATAR		 = 0x4000
  };

  bool is_initialized = false;
  GLuint vao_mesh;
  GLuint vao_overlay_lines;
  GLuint vao_overlay_points;
  GLuint vao_stroke_points;
  GLuint vao_laser_points;
  GLuint vao_hand_point;
  GLuint vao_avatar;

  GLuint shader_mesh;
  GLuint shader_overlay_lines;
  GLuint shader_overlay_points;
  GLuint shader_stroke_points;
  GLuint shader_laser_points;
  GLuint shader_hand_point;
  GLuint shader_avatar;

  GLuint vbo_V; // Vertices of the current mesh (#V x 3)
  GLuint vbo_V_uv; // UV coordinates for the current mesh (#V x 2)
  GLuint vbo_V_normals; // Vertices of the current mesh (#V x 3)
  GLuint vbo_V_ambient; // Ambient material  (#V x 3)
  GLuint vbo_V_diffuse; // Diffuse material  (#V x 3)
  GLuint vbo_V_specular; // Specular material  (#V x 3)

  GLuint vbo_F; // Faces of the mesh (#F x 3)
  GLuint vbo_tex; // Texture

  GLuint vbo_lines_F;         // Indices of the line overlay
  GLuint vbo_lines_V;         // Vertices of the line overlay
  GLuint vbo_lines_V_colors;  // Color values of the line overlay
  GLuint vbo_points_F;        // Indices of the point overlay
  GLuint vbo_points_V;        // Vertices of the point overlay
  GLuint vbo_points_V_colors; // Color values of the point overlay
  GLuint vbo_stroke_points_F;	  // Indices of the stroke overlay
  GLuint vbo_stroke_points_V;	  // Vertices of the stroke overlay
  GLuint vbo_laser_points_F;	  // Indices of the laser overlay
  GLuint vbo_laser_points_V;	  // Vertices of the laser overlay
  GLuint vbo_hand_point_F;
  GLuint vbo_hand_point_V;
  GLuint vbo_hand_point_V_colors;

  GLuint vbo_avatar_V_normals;
  GLuint vbo_avatar_V_tangents;
  GLuint vbo_avatar_V_tex;
  GLuint vbo_avatar_V_poseIndices;
  GLuint vbo_avatar_V_poseWeights;
  //GLuint vbo_avatar_V_colors;
  GLuint vbo_avatar_F;
  GLuint vbo_avatar_V;


  // Temporary copy of the content of each VBO
  typedef Eigen::Matrix<float,Eigen::Dynamic,Eigen::Dynamic,Eigen::RowMajor> RowMatrixXf;
  RowMatrixXf V_vbo;
  RowMatrixXf V_normals_vbo;
  RowMatrixXf V_ambient_vbo;
  RowMatrixXf V_diffuse_vbo;
  RowMatrixXf V_specular_vbo;
  RowMatrixXf V_uv_vbo;
  RowMatrixXf lines_V_vbo;
  RowMatrixXf lines_V_colors_vbo;
  RowMatrixXf points_V_vbo;
  RowMatrixXf points_V_colors_vbo;

  RowMatrixXf stroke_points_V_vbo;
  RowMatrixXf laser_points_V_vbo;
  RowMatrixXf hand_point_V_vbo;
  RowMatrixXf hand_point_V_colors_vbo;

  RowMatrixXf avatar_V_vbo;
  RowMatrixXf avatar_V_normals_vbo;
  RowMatrixXf avatar_V_tangents_vbo;
  RowMatrixXf avatar_V_tex_vbo;
  RowMatrixXf avatar_V_poseIndices_vbo;
  RowMatrixXf avatar_V_poseWeights_vbo;
//  RowMatrixXf avatar_V_colors_vbo;

  int tex_u;
  int tex_v;
  Eigen::Matrix<char,Eigen::Dynamic,1> tex;

  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> F_vbo;
  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> lines_F_vbo;
  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> points_F_vbo;
  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> avatar_F_vbo;
  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic> stroke_points_F_vbo;
  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic> laser_points_F_vbo;
  Eigen::Matrix<unsigned, Eigen::Dynamic, Eigen::Dynamic> hand_point_F_vbo;


  // Marks dirty buffers that need to be uploaded to OpenGL
  uint32_t dirty;

  // Initialize shaders and buffers
  IGL_INLINE void init();

  // Release all resources
  IGL_INLINE void free();

  // Create a new set of OpenGL buffer objects
  IGL_INLINE void init_buffers();

  // Bind the underlying OpenGL buffer objects for subsequent mesh draw calls
  IGL_INLINE void bind_mesh();

  /// Draw the currently buffered mesh (either solid or wireframe)
  IGL_INLINE void draw_mesh(bool solid);

  // Bind the underlying OpenGL buffer objects for subsequent line overlay draw calls
  IGL_INLINE void bind_overlay_lines();

  /// Draw the currently buffered line overlay
  IGL_INLINE void draw_overlay_lines();

  // Bind the underlying OpenGL buffer objects for subsequent point overlay draw calls
  IGL_INLINE void bind_overlay_points();

  /// Draw the currently buffered point overlay
  IGL_INLINE void draw_overlay_points();

  // Bind the underlying OpenGL buffer objects for subsequent stroke overlay draw calls
  IGL_INLINE void bind_stroke();

  /// Draw the currently buffered overlay curves
  IGL_INLINE void draw_stroke();

  // Bind the underlying OpenGL buffer objects for subsequent laser overlay draw calls
  IGL_INLINE void bind_laser();

  /// Draw the currently buffered overlay laser
  IGL_INLINE void draw_laser();

  // Bind the underlying OpenGL buffer objects for subsequent hand marker overlay draw calls
  IGL_INLINE void bind_hand_point();

  /// Draw the currently buffered overlay hand marker
  IGL_INLINE void draw_hand_point();

  IGL_INLINE void bind_avatar();

  // Release the OpenGL buffer objects
  IGL_INLINE void free_buffers();

};

}
}

#ifndef IGL_STATIC_LIBRARY
#  include "MeshGL.cpp"
#endif

#endif
