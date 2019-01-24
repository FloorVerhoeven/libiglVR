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
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9 * nr_segments, volumetric_lines_V_vbo.data(), GL_STATIC_DRAW);
	}

	GLint id_pos = glGetAttribLocation(shader_volumetric_overlay_lines, std::string("position").c_str());
	GLint id_col = glGetAttribLocation(shader_volumetric_overlay_lines, std::string("color").c_str());
	GLint id_nor = glGetAttribLocation(shader_volumetric_overlay_lines, std::string("normal").c_str());

	glEnableVertexAttribArray(id_pos);
	glEnableVertexAttribArray(id_col);
	glEnableVertexAttribArray(id_nor);
	
	GLsizei stride = sizeof(float) * 9;
	const GLvoid* colorOffset = (GLvoid*)(sizeof(float) * 3);
	const GLvoid* normalOffset = (GLvoid*)(sizeof(float) * 6);

	glVertexAttribPointer(id_pos, 3, GL_FLOAT, GL_FALSE, stride, 0);
	glVertexAttribPointer(id_col, 3, GL_FLOAT, GL_FALSE, stride, colorOffset);
	glVertexAttribPointer(id_nor, 3, GL_FLOAT, GL_FALSE, stride, normalOffset);

	glDrawArrays(GL_LINE_STRIP_ADJACENCY_EXT, 0, nr_segments);

	glDisableVertexAttribArray(id_pos);
	glDisableVertexAttribArray(id_col);
	glDisableVertexAttribArray(id_nor);
//	glBindBuffer(GL_ARRAY_BUFFER, 0);
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

  std::string overlay_vertex_shader_string_lines =
	  R"(#version 420
		in vec3 position;
		in vec3 normal;
		in vec3 color;
		uniform mat4 model;
		uniform mat4 view;
		uniform mat4 proj;
		out vec3 vPosition;
		out vec3 vNormal;
		out vec3 vColor_frag;

	void main(){
		gl_Position = proj*view*model*vec4(position, 1.0);
		vPosition = position;
		vNormal = normal;
		vColor_frag = color;
}
)";

  std::string  overlay_geometry_shader_string_lines =
	  R"(
#version 420
#extension GL_EXT_geometry_shader4 : enable

 in vec3 vPosition[4];
 in vec3 vNormal[4];
 in vec3 vColor_frag[4];
 out vec3 gPosition;
 out vec3 gEndpoints[4];
 out vec3 gEndplanes[2];
 out vec3 gColor;
uniform float Radius;
uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

vec4 obb[8];
vec4 obbPrime[8];

bool isFront(int a, int b, int c)
{
    vec3 i = vec3(obbPrime[b].xy - obbPrime[a].xy, 0);
    vec3 j = vec3(obbPrime[c].xy - obbPrime[a].xy, 0);
    return cross(i, j).z > 0.0;
}

void emit(int a, int b, int c, int d)
{
    gPosition = obb[a].xyz; gl_Position = obbPrime[a]; EmitVertex();
    gPosition = obb[b].xyz; gl_Position = obbPrime[b]; EmitVertex();
    gPosition = obb[c].xyz; gl_Position = obbPrime[c]; EmitVertex();
    gPosition = obb[d].xyz; gl_Position = obbPrime[d]; EmitVertex();
}

void main()
{
	mat4 Modelview = view*model;
	gColor = vColor_frag[0];
    // Pass raytracing inputs to fragment shader:
    vec3 p0, p1, p2, p3, n0, n1, n2;
    p0 = (Modelview * vec4(vPosition[0], 1)).xyz;
    p1 = (Modelview * vec4(vPosition[1], 1)).xyz;
    p2 = (Modelview * vec4(vPosition[2], 1)).xyz;
    p3 = (Modelview * vec4(vPosition[3], 1)).xyz;
    n0 = normalize(p1-p0);
    n1 = normalize(p2-p1);
    n2 = normalize(p3-p2);
    gEndpoints[0] = p0; gEndpoints[1] = p1;
    gEndpoints[2] = p2; gEndpoints[3] = p3;
    gEndplanes[0] = normalize(n0+n1);
    gEndplanes[1] = normalize(n1+n2);

    // Compute object-space plane normals:
    p0 = vPosition[0]; p1 = vPosition[1];
    p2 = vPosition[2]; p3 = vPosition[3];
    n0 = normalize(p1-p0);
    n1 = normalize(p2-p1);
    n2 = normalize(p3-p2);
    vec3 u = normalize(n0+n1);
    vec3 v = normalize(n1+n2);

    // Generate a basis for the cuboid:
    vec3 j = n1;
    vec3 i = vNormal[1];
    vec3 k = cross(i, j);

    // Compute the eight corners:
    float r = Radius; float d;
    d = 1.0/dot(u,j); p1 -= j*r*sqrt(d*d-1.0);
    d = 1.0/dot(v,j); p2 += j*r*sqrt(d*d-1.0);
    obb[0] = Modelview*vec4(p1 + i*r + k*r,1);
    obb[1] = Modelview*vec4(p1 + i*r - k*r,1);
    obb[2] = Modelview*vec4(p1 - i*r - k*r,1);
    obb[3] = Modelview*vec4(p1 - i*r + k*r,1);
    obb[4] = Modelview*vec4(p2 + i*r + k*r,1);
    obb[5] = Modelview*vec4(p2 + i*r - k*r,1);
    obb[6] = Modelview*vec4(p2 - i*r - k*r,1);
    obb[7] = Modelview*vec4(p2 - i*r + k*r,1);
    for (int i = 0; i < 8; i++)
        obbPrime[i] = proj * obb[i];
    
    // Emit the front faces of the cuboid:
    if (isFront(0,1,3)) emit(0,1,3,2); EndPrimitive();
    if (isFront(5,4,6)) emit(5,4,6,7); EndPrimitive();
    if (isFront(4,5,0)) emit(4,5,0,1); EndPrimitive();
    if (isFront(3,2,7)) emit(3,2,7,6); EndPrimitive();
    if (isFront(0,3,4)) emit(0,3,4,7); EndPrimitive();
    if (isFront(2,1,6)) emit(2,1,6,5); EndPrimitive();
}
)";


  std::string  overlay_fragment_shader_string_lines =
	  R"(
#version 420
//uniform vec4 Color;

in vec3 gEndpoints[4];
in vec3 gEndplanes[2];
in vec3 gPosition;
in vec3 gColor;

uniform float Radius;
uniform mat4 proj;
uniform vec3 LightDirection;
uniform vec3 DiffuseMaterial;
uniform vec3 AmbientMaterial;
uniform vec3 SpecularMaterial;
uniform float Shininess;
out vec4 out_Color;

vec3 perp(vec3 v)
{
    vec3 b = cross(v, vec3(0, 0, 1));
    if (dot(b, b) < 0.01)
        b = cross(v, vec3(0, 1, 0));
    return b;
}

bool IntersectCylinder(vec3 origin, vec3 dir, out float t)
{
    vec3 A = gEndpoints[1]; vec3 B = gEndpoints[2];
    float Epsilon = 0.0000001;
    float extent = distance(A, B);
    vec3 W = (B - A) / extent;
    vec3 U = perp(W);
    vec3 V = cross(U, W);
    U = normalize(cross(V, W));
    V = normalize(V);
    float rSqr = Radius*Radius;
    vec3 diff = origin - 0.5 * (A + B);
    mat3 basis = mat3(U, V, W);
    vec3 P = diff * basis;
    float dz = dot(W, dir);
    if (abs(dz) >= 1.0 - Epsilon) {
        float radialSqrDist = rSqr - P.x*P.x - P.y*P.y;
        if (radialSqrDist < 0.0)
            return false;
        t = (dz > 0.0 ? -P.z : P.z) + extent * 0.5;
        return true;
    }

    vec3 D = vec3(dot(U, dir), dot(V, dir), dz);
    float a0 = P.x*P.x + P.y*P.y - rSqr;
    float a1 = P.x*D.x + P.y*D.y;
    float a2 = D.x*D.x + D.y*D.y;
    float discr = a1*a1 - a0*a2;
    if (discr < 0.0)
        return false;

    if (discr > Epsilon) {
        float root = sqrt(discr);
        float inv = 1.0/a2;
        t = (-a1 + root)*inv;
        return true;
    }

    t = -a1/a2;
    return true;
}

vec3 ComputeLight(vec3 L, vec3 N, bool specular)
{
    float df = max(0.0,dot(N, L));
    vec3 color = df * DiffuseMaterial;
    if (df > 0.0 && specular) {
        vec3 E = vec3(0, 0, 1);
        vec3 R = reflect(L, N);
        float sf = max(0.0,dot(R, E));
        sf = pow(sf, Shininess);
        color += sf * SpecularMaterial;
    }
    return color;
}

void main()
{
    vec3 rayStart = gPosition;
    vec3 rayEnd = vec3(0);
    vec3 rayDir = normalize(rayEnd - rayStart);

    if (distance(rayStart, rayEnd) < 0.1) {
        discard;
        return;
    }
    
    float d;
    if (!IntersectCylinder(rayStart, rayDir, d)) {
        discard;
       return;
    }

    vec3 hitPoint = rayStart + d * rayDir;
    if (dot(hitPoint - gEndpoints[1], gEndplanes[0]) < 0.0) {
     //  discard;
     //   return;
    }

    if (dot(hitPoint - gEndpoints[2], gEndplanes[1]) > 0.0) {
  //    discard;
    //   return;
    }

    // Compute a lighting normal:
    vec3 x0 = hitPoint;
    vec3 x1 = gEndpoints[1];
    vec3 x2 = gEndpoints[2];
    float length = distance(x1, x2);
    vec3 v = (x2 - x1) / length;
    float t = dot(x0 - x1, v);
    vec3 spinePoint = x1 + t * v;
    vec3 N = -normalize(hitPoint - spinePoint);

    // Perform lighting and write out a new depth value:
    vec3 color = AmbientMaterial + ComputeLight(LightDirection, N, true);
    vec4 ndc = proj * vec4(hitPoint, 1);
    gl_FragDepth = ndc.z / ndc.w;
	out_Color = vec4(color, 1.0);
	//out_Color = vec4(color+gColor, 1.0);
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
  create_shader_program(
	  overlay_geometry_shader_string_lines,
	  overlay_vertex_shader_string_lines,
	  overlay_fragment_shader_string_lines,
	  {},
	  shader_volumetric_overlay_lines);

  //Specific for volumetric lines
  /*
  glProgramParameteriEXT(shader_volumetric_overlay_lines, GL_GEOMETRY_INPUT_TYPE_EXT, GL_LINES_ADJACENCY_EXT);
  glProgramParameteriEXT(shader_volumetric_overlay_lines, GL_GEOMETRY_OUTPUT_TYPE_EXT, GL_TRIANGLE_STRIP);
  glProgramParameteriEXT(shader_volumetric_overlay_lines, GL_GEOMETRY_VERTICES_OUT_EXT, 24);*/
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
