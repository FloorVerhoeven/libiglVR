// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2014 Daniele Panozzo <daniele.panozzo@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef IGL_OPENGL_VIEWERCORE_H
#define IGL_OPENGL_VIEWERCORE_H

#include <igl/opengl/MeshGL.h>
#include <igl/opengl/ViewerData.h>

#include <igl/igl_inline.h>
#include <Eigen/Geometry>
#include <Eigen/Core>

namespace igl
{
namespace opengl
{

// Basic class of the 3D mesh viewer
// TODO: write documentation

class ViewerCore
{
public:
  IGL_INLINE ViewerCore();

  IGL_INLINE ViewerCore& operator = (ViewerCore& other) {
	  std::lock(mu_model, other.mu_model);
	  std::lock_guard<std::mutex> self_lock(mu_model, std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock(other.mu_model, std::adopt_lock);
	  model = other.model;

	  std::lock(mu_view, other.mu_view);
	  std::lock_guard<std::mutex> self_lock2(mu_view, std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock2(other.mu_view, std::adopt_lock);
	  view = other.view;

	  std::lock(mu_proj, other.mu_proj);
	  std::lock_guard<std::mutex> self_lock3(mu_proj, std::adopt_lock);
	  std::lock_guard<std::mutex> other_lock3(other.mu_proj, std::adopt_lock);
	  model = other.proj;

	  background_color = other.background_color;

	  // Lighting
	  light_position = other.light_position;
	  lighting_factor = other.lighting_factor;

	  rotation_type = other.rotation_type;
	  trackball_angle = other.trackball_angle;

	  // Model viewing parameters
	  model_zoom = other.model_zoom;
	  model_translation = other.model_translation;

	  // Model viewing parameters (uv coordinates)
	  model_zoom_uv = other.model_zoom_uv;
	  model_translation_uv = other.model_translation_uv;

	  // Camera parameters
	  camera_zoom = other.camera_zoom;
	  orthographic = other.orthographic;
	  camera_eye = other.camera_eye;
	  camera_up = other.camera_up;
	  camera_center = other.camera_center;
	  camera_view_angle = other.camera_view_angle;
	  camera_dnear = other.camera_dnear;
	  camera_dfar = other.camera_dfar;

	  depth_test = other.depth_test;

	  // Animation
	  is_animating = other.is_animating;
	  animation_max_fps = other.animation_max_fps;

	  // Caches the two-norm between the min/max point of the bounding box
	  object_scale = other.object_scale;

	  // Viewport size
	  viewport = other.viewport;
	  return *this;
  }

  // Initialization
  IGL_INLINE void init();

  // Shutdown
  IGL_INLINE void shut();

  // Serialization code
  IGL_INLINE void InitSerialization();


  // ------------------- Camera control functions

  // Adjust the view to see the entire model
  IGL_INLINE void align_camera_center(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& F);

  // Determines how much to zoom and shift such that the mesh fills the unit
  // box (centered at the origin)
  IGL_INLINE void get_scale_and_shift_to_fit_mesh(
    const Eigen::MatrixXd& V,
    const Eigen::MatrixXi& F,
    float & zoom,
    Eigen::Vector3f& shift);

    // Adjust the view to see the entire model
    IGL_INLINE void align_camera_center(
      const Eigen::MatrixXd& V);

    // Determines how much to zoom and shift such that the mesh fills the unit
    // box (centered at the origin)
    IGL_INLINE void get_scale_and_shift_to_fit_mesh(
      const Eigen::MatrixXd& V,
      float & zoom,
      Eigen::Vector3f& shift);


	IGL_INLINE Eigen::Matrix4f get_model();
	IGL_INLINE Eigen::Matrix4f get_view();
	IGL_INLINE Eigen::Matrix4f get_proj();


  // ------------------- Drawing functions

  // Clear the frame buffers
  IGL_INLINE void clear_framebuffers();

  // Draw everything
  //
  // data cannot be const because it is being set to "clean"
  IGL_INLINE void draw(ViewerData& data, bool update_matrices = true, bool oculusVR = false, Eigen::Matrix4f& view_ = NULL, Eigen::Matrix4f& proj_ = NULL);
  IGL_INLINE void draw_buffer(
    ViewerData& data,
    bool update_matrices,
    Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& R,
    Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& G,
    Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& B,
    Eigen::Matrix<unsigned char,Eigen::Dynamic,Eigen::Dynamic>& A);

  // Trackball angle (quaternion)
  enum RotationType
  {
    ROTATION_TYPE_TRACKBALL = 0,
    ROTATION_TYPE_TWO_AXIS_VALUATOR_FIXED_UP = 1,
    ROTATION_TYPE_NO_ROTATION = 2,
    NUM_ROTATION_TYPES = 3
  };
  IGL_INLINE void set_rotation_type(const RotationType & value);

  // ------------------- Properties

  // Colors
  Eigen::Vector4f background_color;

  // Lighting
  Eigen::Vector3f light_position;
  float lighting_factor;

  RotationType rotation_type;

  Eigen::Quaternionf trackball_angle;

  // Model viewing parameters
  float model_zoom;
  Eigen::Vector3f model_translation;

  // Model viewing parameters (uv coordinates)
  float model_zoom_uv;
  Eigen::Vector3f model_translation_uv;

  // Camera parameters
  float camera_zoom;
  bool orthographic;
  Eigen::Vector3f camera_eye;
  Eigen::Vector3f camera_up;
  Eigen::Vector3f camera_center;
  float camera_view_angle;
  float camera_dnear;
  float camera_dfar;

  bool depth_test;

  // Animation
  bool is_animating;
  double animation_max_fps;

  // Caches the two-norm between the min/max point of the bounding box
  float object_scale;

  // Viewport size
  Eigen::Vector4f viewport;

  // Save the OpenGL transformation matrices used for the previous rendering
  // pass
  Eigen::Matrix4f view;
  Eigen::Matrix4f model;
  Eigen::Matrix4f proj;

  mutable std::mutex mu_model;
  mutable std::mutex mu_view;
  mutable std::mutex mu_proj;

  public:
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

}
}

#include <igl/serialize.h>
namespace igl {
  namespace serialization {

    inline void serialization(bool s, igl::opengl::ViewerCore& obj, std::vector<char>& buffer)
    {

      SERIALIZE_MEMBER(background_color);

      SERIALIZE_MEMBER(light_position);
      SERIALIZE_MEMBER(lighting_factor);

      SERIALIZE_MEMBER(trackball_angle);
      SERIALIZE_MEMBER(rotation_type);

      SERIALIZE_MEMBER(model_zoom);
      SERIALIZE_MEMBER(model_translation);

      SERIALIZE_MEMBER(model_zoom_uv);
      SERIALIZE_MEMBER(model_translation_uv);

      SERIALIZE_MEMBER(camera_zoom);
      SERIALIZE_MEMBER(orthographic);
      SERIALIZE_MEMBER(camera_view_angle);
      SERIALIZE_MEMBER(camera_dnear);
      SERIALIZE_MEMBER(camera_dfar);
      SERIALIZE_MEMBER(camera_eye);
      SERIALIZE_MEMBER(camera_center);
      SERIALIZE_MEMBER(camera_up);

      SERIALIZE_MEMBER(depth_test);
      SERIALIZE_MEMBER(is_animating);
      SERIALIZE_MEMBER(animation_max_fps);

      SERIALIZE_MEMBER(object_scale);

      SERIALIZE_MEMBER(viewport);
      SERIALIZE_MEMBER(view);
      SERIALIZE_MEMBER(model);
      SERIALIZE_MEMBER(proj);

	//  SERIALIZE_MEMBER(mu_model);
	//  SERIALIZE_MEMBER(mu_view);
	//  SERIALIZE_MEMBER(mu_proj);

    }

    template<>
    inline void serialize(const igl::opengl::ViewerCore& obj, std::vector<char>& buffer)
    {
      serialization(true, const_cast<igl::opengl::ViewerCore&>(obj), buffer);
    }

    template<>
    inline void deserialize(igl::opengl::ViewerCore& obj, const std::vector<char>& buffer)
    {
      serialization(false, obj, const_cast<std::vector<char>&>(buffer));
    }
  }
}

#ifndef IGL_STATIC_LIBRARY
#  include "ViewerCore.cpp"
#endif

#endif
