// This file is part of libigl, a simple c++ geometry processing library.
//
// Copyright (C) 2014 Daniele Panozzo <daniele.panozzo@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public License
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at http://mozilla.org/MPL/2.0/.

#include "MeshGL.h"
#include "bind_vertex_attrib_array.h"
#include "create_shader_program.h"
#include "destroy_shader_program.h"
#include <iostream>

IGL_INLINE void igl::opengl::MeshGL::init_buffers()
{
  // Mesh: Vertex Array Object & Buffer objects
  glGenVertexArrays(1, &vao_mesh);
  glBindVertexArray(vao_mesh);
  glGenBuffers(1, &vbo_V);
  glGenBuffers(1, &vbo_V_normals);
  glGenBuffers(1, &vbo_V_ambient);
  glGenBuffers(1, &vbo_V_diffuse);
  glGenBuffers(1, &vbo_V_specular);
  glGenBuffers(1, &vbo_V_uv);
  glGenBuffers(1, &vbo_F);
  glGenTextures(1, &vbo_tex);

  // Line overlay
  glGenVertexArrays(1, &vao_overlay_lines);
  glBindVertexArray(vao_overlay_lines);
  glGenBuffers(1, &vbo_lines_F);
  glGenBuffers(1, &vbo_lines_V);
  glGenBuffers(1, &vbo_lines_V_colors);
  glGenBuffers(1, &vbo_lines_V_normals);

  // Point overlay
  glGenVertexArrays(1, &vao_overlay_points);
  glBindVertexArray(vao_overlay_points);
  glGenBuffers(1, &vbo_points_F);
  glGenBuffers(1, &vbo_points_V);
  glGenBuffers(1, &vbo_points_V_colors);

  // Laser overlay
  glGenVertexArrays(1, &vao_laser_points);
  glBindVertexArray(vao_laser_points);
  glGenBuffers(1, &vbo_laser_points_F);
  glGenBuffers(1, &vbo_laser_points_V);
  glGenBuffers(1, &vbo_laser_V_colors);


  // Linestrip overlay
  glGenVertexArrays(1, &vao_overlay_strip);
  glBindVertexArray(vao_overlay_strip);
  glGenBuffers(1, &vbo_overlay_strip_F);
  glGenBuffers(1, &vbo_overlay_strip_V);
  glGenBuffers(1, &vbo_overlay_strip_V_colors);

  // Hand marker overlay
  glGenVertexArrays(1, &vao_hand_point);
  glBindVertexArray(vao_hand_point);
  glGenBuffers(1, &vbo_hand_point_F);
  glGenBuffers(1, &vbo_hand_point_V);
  glGenBuffers(1, &vbo_hand_point_V_colors);

  //Volumetric line overlay
  glGenVertexArrays(1, &vao_volumetric_overlay_lines);
  glBindVertexArray(vao_volumetric_overlay_lines);
  glGenBuffers(1, &vbo_volumetric_lines_F);
  glGenBuffers(1, &vbo_volumetric_lines_V);
  glGenBuffers(1, &vbo_volumetric_lines_colors);
  glGenBuffers(1, &vbo_volumetric_lines_normals);
  dirty = MeshGL::DIRTY_ALL;
}

IGL_INLINE void igl::opengl::MeshGL::free_buffers()
{
  if (is_initialized)
  {
    glDeleteVertexArrays(1, &vao_mesh);
    glDeleteVertexArrays(1, &vao_overlay_lines);
    glDeleteVertexArrays(1, &vao_overlay_points);
	glDeleteVertexArrays(1, &vao_laser_points);
	glDeleteVertexArrays(1, &vao_overlay_strip);
	glDeleteVertexArrays(1, &vao_hand_point);
	glDeleteVertexArrays(1, &vao_volumetric_overlay_lines);

    glDeleteBuffers(1, &vbo_V);
    glDeleteBuffers(1, &vbo_V_normals);
    glDeleteBuffers(1, &vbo_V_ambient);
    glDeleteBuffers(1, &vbo_V_diffuse);
    glDeleteBuffers(1, &vbo_V_specular);
    glDeleteBuffers(1, &vbo_V_uv);
    glDeleteBuffers(1, &vbo_F);
    glDeleteBuffers(1, &vbo_lines_F);
    glDeleteBuffers(1, &vbo_lines_V);
    glDeleteBuffers(1, &vbo_lines_V_colors);
	glDeleteBuffers(1, &vbo_lines_V_normals);
    glDeleteBuffers(1, &vbo_points_F);
    glDeleteBuffers(1, &vbo_points_V);
    glDeleteBuffers(1, &vbo_points_V_colors);
	glDeleteBuffers(1, &vbo_laser_points_V);
	glDeleteBuffers(1, &vbo_laser_points_F);
	glDeleteBuffers(1, &vbo_laser_V_colors);
	glDeleteBuffers(1, &vbo_overlay_strip_F);
	glDeleteBuffers(1, &vbo_overlay_strip_V);
	glDeleteBuffers(1, &vbo_overlay_strip_V_colors);
	glDeleteBuffers(1, &vbo_hand_point_F);
	glDeleteBuffers(1, &vbo_hand_point_V);
	glDeleteBuffers(1, &vbo_hand_point_V_colors);
	glDeleteBuffers(1, &vbo_volumetric_lines_F);
	glDeleteBuffers(1, &vbo_volumetric_lines_V);
	glDeleteBuffers(1, &vbo_volumetric_lines_colors);
	glDeleteBuffers(1, &vbo_volumetric_lines_normals);

    glDeleteTextures(1, &vbo_tex);
  }
}

IGL_INLINE void igl::opengl::MeshGL::bind_mesh()
{
  glBindVertexArray(vao_mesh);
  glUseProgram(shader_mesh);
  bind_vertex_attrib_array(shader_mesh,"position", vbo_V, V_vbo, dirty & MeshGL::DIRTY_POSITION);
  bind_vertex_attrib_array(shader_mesh,"normal", vbo_V_normals, V_normals_vbo, dirty & MeshGL::DIRTY_NORMAL);
  bind_vertex_attrib_array(shader_mesh,"Ka", vbo_V_ambient, V_ambient_vbo, dirty & MeshGL::DIRTY_AMBIENT);
  bind_vertex_attrib_array(shader_mesh,"Kd", vbo_V_diffuse, V_diffuse_vbo, dirty & MeshGL::DIRTY_DIFFUSE);
  bind_vertex_attrib_array(shader_mesh,"Ks", vbo_V_specular, V_specular_vbo, dirty & MeshGL::DIRTY_SPECULAR);
  bind_vertex_attrib_array(shader_mesh,"texcoord", vbo_V_uv, V_uv_vbo, dirty & MeshGL::DIRTY_UV);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_F);
  if (dirty & MeshGL::DIRTY_FACE)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*F_vbo.size(), F_vbo.data(), GL_DYNAMIC_DRAW);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, vbo_tex);
  glGenerateMipmap(GL_TEXTURE_2D);

  if (dirty & MeshGL::DIRTY_TEXTURE)
  {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_u, tex_v, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data());
  }
  glUniform1i(glGetUniformLocation(shader_mesh,"tex"), 0);
  dirty &= ~MeshGL::DIRTY_MESH;
}

IGL_INLINE void igl::opengl::MeshGL::bind_overlay_lines()
{
  bool is_dirty = dirty & MeshGL::DIRTY_OVERLAY_LINES;

  glBindVertexArray(vao_overlay_lines);
  glUseProgram(shader_overlay_lines);
 bind_vertex_attrib_array(shader_overlay_lines,"position", vbo_lines_V, lines_V_vbo, is_dirty);
 bind_vertex_attrib_array(shader_overlay_lines,"color", vbo_lines_V_colors, lines_V_colors_vbo, is_dirty);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_lines_F);
  if (is_dirty)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*lines_F_vbo.size(), lines_F_vbo.data(), GL_DYNAMIC_DRAW);

  dirty &= ~MeshGL::DIRTY_OVERLAY_LINES;
}

IGL_INLINE void igl::opengl::MeshGL::bind_overlay_points()
{
  bool is_dirty = dirty & MeshGL::DIRTY_OVERLAY_POINTS;

  glBindVertexArray(vao_overlay_points);
  glUseProgram(shader_overlay_points);

 bind_vertex_attrib_array(shader_overlay_points,"position", vbo_points_V, points_V_vbo, is_dirty);
 bind_vertex_attrib_array(shader_overlay_points,"color", vbo_points_V_colors, points_V_colors_vbo, is_dirty);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_points_F);
  if (is_dirty)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*points_F_vbo.size(), points_F_vbo.data(), GL_DYNAMIC_DRAW);

  dirty &= ~MeshGL::DIRTY_OVERLAY_POINTS;
}

IGL_INLINE void igl::opengl::MeshGL::bind_laser() {
	bool is_dirty = dirty & MeshGL::DIRTY_LASER;

	glBindVertexArray(vao_laser_points);
	glUseProgram(shader_overlay_lines);

	bind_vertex_attrib_array(shader_overlay_lines,"position", vbo_laser_points_V, laser_points_V_vbo, is_dirty);
	bind_vertex_attrib_array(shader_overlay_lines, "color", vbo_laser_V_colors, laser_V_colors_vbo, is_dirty);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_laser_points_F);
	if (is_dirty) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*laser_points_F_vbo.size(), laser_points_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_LASER;
}

IGL_INLINE void igl::opengl::MeshGL::bind_overlay_linestrip() {
	bool is_dirty = dirty & MeshGL::DIRTY_OVERLAY_STRIP;

	glBindVertexArray(vao_overlay_strip);
	glUseProgram(shader_overlay_lines);

	bind_vertex_attrib_array(shader_overlay_lines, "position", vbo_overlay_strip_V, overlay_strip_V_vbo, is_dirty);
	bind_vertex_attrib_array(shader_overlay_lines, "color", vbo_overlay_strip_V_colors, overlay_strip_V_colors_vbo, is_dirty);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_overlay_strip_F);
	if (is_dirty) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*overlay_strip_F_vbo.size(), overlay_strip_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_OVERLAY_STRIP;
}

IGL_INLINE void igl::opengl::MeshGL::bind_hand_point()
{
	bool is_dirty = dirty & MeshGL::DIRTY_HAND_POINT;

	glBindVertexArray(vao_hand_point);  
	glUseProgram(shader_overlay_points);
	bind_vertex_attrib_array(shader_overlay_points, "position", vbo_hand_point_V, hand_point_V_vbo, is_dirty);
	bind_vertex_attrib_array(shader_overlay_points,"color", vbo_hand_point_V_colors, hand_point_V_colors_vbo, is_dirty);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_hand_point_F);

	if (is_dirty) {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned)*hand_point_F_vbo.size(), hand_point_F_vbo.data(), GL_DYNAMIC_DRAW);
	}

	dirty &= ~MeshGL::DIRTY_HAND_POINT;
}

IGL_INLINE void igl::opengl::MeshGL::bind_volumetric_lines() {
	bool is_dirty = dirty & MeshGL::DIRTY_VOLUMETRIC_LINES;

	int nr_segments = volumetric_lines_V_vbo.rows();
	glBindBuffer(GL_ARRAY_BUFFER, vbo_volumetric_lines_V);
	if (is_dirty) {
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * nr_segments, volumetric_lines_V_vbo.data(), GL_STATIC_DRAW);
	}

	GLint id_pos = glGetAttribLocation(shader_volumetric_overlay_lines, std::string("position").c_str());
	GLint id_nor = glGetAttribLocation(shader_volumetric_overlay_lines, std::string("normal").c_str());
	glEnableVertexAttribArray(id_pos);
	glEnableVertexAttribArray(id_nor);
	GLsizei stride = sizeof(float) * 6;
	const GLvoid* normalOffset = (GLvoid*)(sizeof(float) * 3);
	glVertexAttribPointer(id_pos, 3, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(id_nor, 3, GL_FLOAT, GL_FALSE, stride, normalOffset);

	glDrawArrays(GL_LINE_STRIP_ADJACENCY_EXT, 0, nr_segments);
	glDisableVertexAttribArray(id_pos);
	glDisableVertexAttribArray(id_nor);
	
	dirty &= ~MeshGL::DIRTY_VOLUMETRIC_LINES;
}

IGL_INLINE void igl::opengl::MeshGL::draw_mesh(bool solid)
{
  glPolygonMode(GL_FRONT_AND_BACK, solid ? GL_FILL : GL_LINE);

  /* Avoid Z-buffer fighting between filled triangles & wireframe lines */
  if (solid)
  {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);
  }
  else {
	  glEnable(GL_POLYGON_OFFSET_LINE);
	  glPolygonOffset(0.5, 0.5); //Pushes the wireframe back as well, but slightly less than the filled triangles. Used to avoid z-buffer fighting with overlay lines (stroke) that are placed on top of the wireframes
  }
  glDrawElements(GL_TRIANGLES, 3*F_vbo.rows(), GL_UNSIGNED_INT, 0);

  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

IGL_INLINE void igl::opengl::MeshGL::draw_overlay_lines()
{
  glDrawElements(GL_LINES, lines_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_overlay_points()
{
  glDrawElements(GL_POINTS, points_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_laser()
{
	glDrawElements(GL_LINE_STRIP, laser_points_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_overlay_linestrip() {
	glDrawElements(GL_LINE_STRIP, overlay_strip_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::draw_hand_point()
{
	glDrawElements(GL_POINTS, hand_point_F_vbo.rows(), GL_UNSIGNED_INT, 0);
}

IGL_INLINE void igl::opengl::MeshGL::init()
{
  if(is_initialized)
  {
    return;
  }
  is_initialized = true;
  std::string mesh_vertex_shader_string =
R"(#version 150
  uniform mat4 model;
  uniform mat4 view;
  uniform mat4 proj;
  in vec3 position;
  in vec3 normal;
  out vec3 position_eye;
  out vec3 normal_eye;
  in vec4 Ka;
  in vec4 Kd;
  in vec4 Ks;
  in vec2 texcoord;
  out vec2 texcoordi;
  out vec4 Kai;
  out vec4 Kdi;
  out vec4 Ksi;

  void main()
  {
    position_eye = vec3 (view * model * vec4 (position, 1.0));
    normal_eye = vec3 (view * model * vec4 (normal, 0.0));
    normal_eye = normalize(normal_eye);
    gl_Position = proj * vec4 (position_eye, 1.0); //proj * view * model * vec4(position, 1.0);
    Kai = Ka;
    Kdi = Kd;
    Ksi = Ks;
    texcoordi = texcoord;
  }
)";

  std::string mesh_fragment_shader_string = 
R"(#version 150
  uniform mat4 model;
  uniform mat4 view;
  uniform mat4 proj;
  uniform vec4 fixed_color;
  in vec3 position_eye;
  in vec3 normal_eye;
  uniform vec3 light_position_world;
  vec3 Ls = vec3 (1, 1, 1);
  vec3 Ld = vec3 (1, 1, 1);
  vec3 La = vec3 (1, 1, 1);
  in vec4 Ksi;
  in vec4 Kdi;
  in vec4 Kai;
  in vec2 texcoordi;
  uniform sampler2D tex;
  uniform float specular_exponent;
  uniform float lighting_factor;
  uniform float texture_factor;
  out vec4 outColor;
  void main()
  {
    vec3 Ia = La * vec3(Kai);    // ambient intensity

    vec3 light_position_eye = vec3 (view * vec4 (light_position_world, 1.0));
    vec3 vector_to_light_eye = light_position_eye - position_eye;
    vec3 direction_to_light_eye = normalize (vector_to_light_eye);
    float dot_prod = dot (direction_to_light_eye, normal_eye);
    float clamped_dot_prod = max (dot_prod, 0.0);
    vec3 Id = Ld * vec3(Kdi) * clamped_dot_prod;    // Diffuse intensity

    vec3 reflection_eye = reflect (-direction_to_light_eye, normal_eye);
    vec3 surface_to_viewer_eye = normalize (-position_eye);
    float dot_prod_specular = dot (reflection_eye, surface_to_viewer_eye);
    dot_prod_specular = float(abs(dot_prod)==dot_prod) * max (dot_prod_specular, 0.0);
    float specular_factor = pow (dot_prod_specular, specular_exponent);
    vec3 Is = Ls * vec3(Ksi) * specular_factor;    // specular intensity
    vec4 color = vec4(lighting_factor * (Is + Id) + Ia + (1.0-lighting_factor) * vec3(Kdi),(Kai.a+Ksi.a+Kdi.a)/3);
    outColor = mix(vec4(1,1,1,1), texture(tex, texcoordi), texture_factor) * color;
    if (fixed_color != vec4(0.0)) outColor = fixed_color;
  }
  )";

  std::string overlay_vertex_shader_string =
R"(#version 150
  uniform mat4 model;
  uniform mat4 view;
  uniform mat4 proj;
  in vec3 position;
  in vec3 color;
  out vec3 color_frag;

  void main()
  {
    gl_Position = proj * view * model * vec4 (position, 1.0);
    color_frag = color;
  }
)";

  std::string overlay_fragment_shader_string =
R"(#version 150
  in vec3 color_frag;
  out vec4 outColor;
  void main()
  {
    outColor = vec4(color_frag, 1.0);
  }
)";

  std::string overlay_point_fragment_shader_string =
R"(#version 150
  in vec3 color_frag;
  out vec4 outColor;
  void main()
  {
    if (length(gl_PointCoord - vec2(0.5)) > 0.5)
      discard;
    outColor = vec4(color_frag, 1.0);
  }
)"; 

  init_buffers();
  create_shader_program(
    mesh_vertex_shader_string,
    mesh_fragment_shader_string,
    {},
    shader_mesh);
  create_shader_program(
    overlay_vertex_shader_string,
    overlay_fragment_shader_string,
    {},
    shader_overlay_lines);
  create_shader_program(
    overlay_vertex_shader_string,
    overlay_point_fragment_shader_string,
    {},
    shader_overlay_points);
 
}

IGL_INLINE void igl::opengl::MeshGL::free()
{
  const auto free = [](GLuint & id)
  {
    if(id)
    {
      destroy_shader_program(id);
      id = 0;
    }
  };

  if (is_initialized)
  {
    free(shader_mesh);
    free(shader_overlay_lines);
    free(shader_overlay_points);
	free(shader_volumetric_overlay_lines);
    free_buffers();
  }
}
