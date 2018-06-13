#define IGL_OCULUSVR_CPP
#include "OculusVR.h"
#include <igl/opengl/glfw/Viewer.h>
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>

#ifdef _WIN32
#  include <windows.h>
#  undef max
#  undef min
#endif

#define FAIL(X) throw std::runtime_error(X)
#define APP_ID "2094457250596307"

#include <igl/igl_inline.h>
#include <igl/two_axis_valuator_fixed_up.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_CAPI_Util.h>
#include <Extras/OVR_StereoProjection.h>
#include "Extras/OVR_Math.h"
#include <igl/opengl/gl.h> //TODO: remove afterwards. only used for error checking
#include <igl/opengl/glfw/imgui/ImGuiMenu.h>
#include <igl/ray_mesh_intersect.h>

using namespace Eigen;
using namespace OVR;

namespace igl {
	namespace opengl {
		static ovrSession session;
		static ovrHmdDesc hmdDesc;
		static ovrGraphicsLuid luid;
		static ovrMirrorTexture _mirrorTexture;
		static ovrMirrorTextureDesc mirror_desc;
		static ovrTrackingState hmdState;
		static ovrPosef EyeRenderPose[2];
		static ovrEyeRenderDesc eyeRenderDesc[2];
		static unsigned int frame{ 0 };
		static double sensorSampleTime;
		static GLuint _mirrorFbo{ 0 };
		static GLuint _skinnedMeshProgram;

		static bool menu_active = false;
		static float menu_z_pos = -1.50f;
		static float pixels_to_meter = 0.001013f; //Transform factor for going from pixels to meters in Oculus

		static ovrAvatar* _avatar;
		static bool _combineMeshes = true;
		static bool _waitingOnCombinedMesh = false;
		static size_t _loadingAssets;
		static ovrAvatarAssetID _avatarCombinedMeshAlpha = 0;
		static ovrAvatarVector4f _avatarCombinedMeshAlphaOffset;
		static std::map<ovrAvatarAssetID, void*> _assetMap;
		static float _elapsedSeconds;
		static ovrHapticsBuffer _hapticBuffer;


		IGL_INLINE void OculusVR::init() {
			eye_pos_lock = std::unique_lock<std::mutex>(mu_last_eye_origin, std::defer_lock);
			touch_dir_lock = std::unique_lock<std::mutex>(mu_touch_dir, std::defer_lock);
			callback_button_down = nullptr;
			callback_menu_opened = nullptr;
			callback_menu_closed = nullptr;
			callback_GUI_set_mouse = nullptr;
			callback_GUI_button_press = nullptr;
			callback_GUI_button_release = nullptr;

			// Initialize the OVR Platform module
			if (!OVR_SUCCESS(ovr_PlatformInitializeWindows(APP_ID))) {
				FAIL("Failed to initialize the Oculus platform");
			}

			// Create OVR Session Object
			if (!OVR_SUCCESS(ovr_Initialize(nullptr))) {
				FAIL("Failed to initialize the Oculus SDK");
			}

			//create OVR session & HMD-description
			if (!OVR_SUCCESS(ovr_Create(&session, &luid))) {
				FAIL("Unable to create HMD session");
			}

			hmdDesc = ovr_GetHmdDesc(session);
			ovrSizei windowSize = { hmdDesc.Resolution.w / 2, hmdDesc.Resolution.h / 2 };

			_skinnedMeshProgram = get_skinned_avatar_shader();

			if (!init_VR_buffers(windowSize.w, windowSize.h)) {
				FAIL("Something went wrong when initializing VR buffers");
			}

			// Initialize the avatar module
			ovrAvatar_Initialize(APP_ID);
			ovrID userID = ovr_GetLoggedInUserID();

			auto requestSpec = ovrAvatarSpecificationRequest_Create(userID);
			ovrAvatarSpecificationRequest_SetCombineMeshes(requestSpec, _combineMeshes);
			ovrAvatar_RequestAvatarSpecificationFromSpecRequest(requestSpec);
			ovrAvatarSpecificationRequest_Destroy(requestSpec);

			ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);
			ovr_RecenterTrackingOrigin(session);

			on_render_start();
			OVR::Vector3f eyePos = EyeRenderPose[1].Position;
			eye_pos_lock.lock();
			current_eye_origin = to_Eigen(eyePos);
			eye_pos_lock.unlock();

			for (int i = 0; i < hapticBufferSize; i++) {
				hapticBuffer[i] = (unsigned char)0;
			}
			for (int i = 0; i < 5; i++) {
				hapticBuffer[i] = (unsigned char)255;
			}
			_hapticBuffer.SubmitMode = ovrHapticsBufferSubmit_Enqueue;
			_hapticBuffer.SamplesCount = hapticBufferSize;
			_hapticBuffer.Samples = (void *)hapticBuffer;

			menu_lastTime = std::chrono::steady_clock::now();

			prev_press = NONE;
			prev_sent = NONE;
			count = 0;
		}

		IGL_INLINE GLuint OculusVR::get_skinned_avatar_shader() {
			std::string avatar_vertex_shader_string =
				R"(#version 330 core
#extension GL_ARB_separate_shader_objects : enable
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 tangent;
layout (location = 3) in vec2 texCoord;
layout (location = 4) in vec4 poseIndices;
layout (location = 5) in vec4 poseWeights;
layout (location = 6) in vec4 color;
out vec3 vertexWorldPos;
out vec3 vertexViewDir;
out vec3 vertexObjPos;
out vec3 vertexNormal;
out vec3 vertexTangent;
out vec3 vertexBitangent;
out vec2 vertexUV;
out vec4 vertexColor;
uniform vec3 viewPos;
uniform mat4 world;
uniform mat4 viewProj;
uniform mat4 meshPose[64];
void main() {
    vec4 vertexPose;
    vertexPose = meshPose[int(poseIndices[0])] * vec4(position, 1.0) * poseWeights[0];
    vertexPose += meshPose[int(poseIndices[1])] * vec4(position, 1.0) * poseWeights[1];
    vertexPose += meshPose[int(poseIndices[2])] * vec4(position, 1.0) * poseWeights[2];
    vertexPose += meshPose[int(poseIndices[3])] * vec4(position, 1.0) * poseWeights[3];
    
    vec4 normalPose;
    normalPose = meshPose[int(poseIndices[0])] * vec4(normal, 0.0) * poseWeights[0];
    normalPose += meshPose[int(poseIndices[1])] * vec4(normal, 0.0) * poseWeights[1];
    normalPose += meshPose[int(poseIndices[2])] * vec4(normal, 0.0) * poseWeights[2];
    normalPose += meshPose[int(poseIndices[3])] * vec4(normal, 0.0) * poseWeights[3];
    normalPose = normalize(normalPose);

	vec4 tangentPose;
    tangentPose = meshPose[int(poseIndices[0])] * vec4(tangent.xyz, 0.0) * poseWeights[0];
    tangentPose += meshPose[int(poseIndices[1])] * vec4(tangent.xyz, 0.0) * poseWeights[1];
    tangentPose += meshPose[int(poseIndices[2])] * vec4(tangent.xyz, 0.0) * poseWeights[2];
    tangentPose += meshPose[int(poseIndices[3])] * vec4(tangent.xyz, 0.0) * poseWeights[3];
	tangentPose = normalize(tangentPose);

	vertexWorldPos = vec3(world * vertexPose);
	gl_Position = viewProj * vec4(vertexWorldPos, 1.0);
	vertexViewDir = normalize(viewPos - vertexWorldPos.xyz);
    vertexObjPos = position.xyz;
	vertexNormal = (world * normalPose).xyz;
	vertexTangent = (world * tangentPose).xyz;
	vertexBitangent = normalize(cross(vertexNormal, vertexTangent) * tangent.w);
	vertexUV = texCoord;
	vertexColor = color.rgba;
})";

			std::string avatar_fragment_shader_string =
				R"( #version 330 core

#define SAMPLE_MODE_COLOR 0
#define SAMPLE_MODE_TEXTURE 1
#define SAMPLE_MODE_TEXTURE_SINGLE_CHANNEL 2
#define SAMPLE_MODE_PARALLAX 3
#define SAMPLE_MODE_RSRM 4

#define MASK_TYPE_NONE 0
#define MASK_TYPE_POSITIONAL 1
#define MASK_TYPE_REFLECTION 2
#define MASK_TYPE_FRESNEL 3
#define MASK_TYPE_PULSE 4

#define BLEND_MODE_ADD 0
#define BLEND_MODE_MULTIPLY 1

#define MAX_LAYER_COUNT 8

	  in vec3 vertexWorldPos;
  in vec3 vertexViewDir;
  in vec3 vertexObjPos;
  in vec3 vertexNormal;
  in vec3 vertexTangent;
  in vec3 vertexBitangent;
  in vec2 vertexUV;
  out vec4 fragmentColor;

  uniform vec4 baseColor;
  uniform int baseMaskType;
  uniform vec4 baseMaskParameters;
  uniform vec4 baseMaskAxis;
  uniform sampler2D alphaMask;
  uniform vec4 alphaMaskScaleOffset;
  uniform sampler2D normalMap;
  uniform vec4 normalMapScaleOffset;
  uniform sampler2D parallaxMap;
  uniform vec4 parallaxMapScaleOffset;
  uniform sampler2D roughnessMap;
  uniform vec4 roughnessMapScaleOffset;

  uniform mat4 projectorInv;

  uniform bool useAlpha;
  uniform bool useNormalMap;
  uniform bool useRoughnessMap;
  uniform bool useProjector;
  uniform float elapsedSeconds;

  uniform int layerCount;

  uniform int layerSamplerModes[MAX_LAYER_COUNT];
  uniform int layerBlendModes[MAX_LAYER_COUNT];
  uniform int layerMaskTypes[MAX_LAYER_COUNT];
  uniform vec4 layerColors[MAX_LAYER_COUNT];
  uniform sampler2D layerSurfaces[MAX_LAYER_COUNT];
  uniform vec4 layerSurfaceScaleOffsets[MAX_LAYER_COUNT];
  uniform vec4 layerSampleParameters[MAX_LAYER_COUNT];
  uniform vec4 layerMaskParameters[MAX_LAYER_COUNT];
  uniform vec4 layerMaskAxes[MAX_LAYER_COUNT];

  vec3 ComputeNormal(mat3 tangentTransform, vec3 worldNormal, vec3 surfaceNormal, float surfaceStrength)
  {
	  if (useNormalMap)
	  {
		  vec3 surface = mix(vec3(0.0, 0.0, 1.0), surfaceNormal, surfaceStrength);
		  return normalize(surface * tangentTransform);
	  }
	  else
	  {
		  return worldNormal;
	  }
  }

  vec3 ComputeColor(int sampleMode, vec2 uv, vec4 color, sampler2D surface, vec4 surfaceScaleOffset, vec4 sampleParameters, mat3 tangentTransform, vec3 worldNormal, vec3 surfaceNormal)
  {
	  if (sampleMode == SAMPLE_MODE_TEXTURE)
	  {
		  vec2 panning = elapsedSeconds * sampleParameters.xy;
		  return texture(surface, (uv + panning) * surfaceScaleOffset.xy + surfaceScaleOffset.zw).rgb * color.rgb;
	  }
	  else if (sampleMode == SAMPLE_MODE_TEXTURE_SINGLE_CHANNEL)
	  {
		  vec4 channelMask = sampleParameters;
		  vec4 channels = texture(surface, uv * surfaceScaleOffset.xy + surfaceScaleOffset.zw);
		  return dot(channelMask, channels) * color.rgb;
	  }
	  else if (sampleMode == SAMPLE_MODE_PARALLAX)
	  {
		  float parallaxMinHeight = sampleParameters.x;
		  float parallaxMaxHeight = sampleParameters.y;
		  float parallaxValue = texture(parallaxMap, uv * parallaxMapScaleOffset.xy + parallaxMapScaleOffset.zw).r;
		  float scaledHeight = mix(parallaxMinHeight, parallaxMaxHeight, parallaxValue);
		  vec2 parallaxUV = (vertexViewDir * tangentTransform).xy * scaledHeight;

		  return texture(surface, (uv + parallaxUV) * surfaceScaleOffset.xy + surfaceScaleOffset.zw).rgb * color.rgb;
	  }
	  else if (sampleMode == SAMPLE_MODE_RSRM)
	  {
		  float roughnessMin = sampleParameters.x;
		  float roughnessMax = sampleParameters.y;

		  float scaledRoughness = roughnessMin;
		  if (useRoughnessMap)
		  {
			  float roughnessValue = texture(roughnessMap, uv * roughnessMapScaleOffset.xy + roughnessMapScaleOffset.zw).r;
			  scaledRoughness = mix(roughnessMin, roughnessMax, roughnessValue);
		  }

		  float normalMapStrength = sampleParameters.z;
		  vec3 viewReflect = reflect(-vertexViewDir, ComputeNormal(tangentTransform, worldNormal, surfaceNormal, normalMapStrength));
		  float viewAngle = viewReflect.y * 0.5 + 0.5;
		  return texture(surface, vec2(scaledRoughness, viewAngle)).rgb * color.rgb;
	  }
	  return color.rgb;
  }

  float ComputeMask(int maskType, vec4 maskParameters, vec4 maskAxis, mat3 tangentTransform, vec3 worldNormal, vec3 surfaceNormal)
  {
	  if (maskType == MASK_TYPE_POSITIONAL) {
		  float centerDistance = maskParameters.x;
		  float fadeAbove = maskParameters.y;
		  float fadeBelow = maskParameters.z;
		  float d = dot(vertexObjPos, maskAxis.xyz);
		  if (d > centerDistance) {
			  return clamp(1.0 - (d - centerDistance) / fadeAbove, 0.0, 1.0);
		  }
		  else {
			  return clamp(1.0 - (centerDistance - d) / fadeBelow, 0.0, 1.0);
		  }
	  }
	  else if (maskType == MASK_TYPE_REFLECTION) {
		  float fadeStart = maskParameters.x;
		  float fadeEnd = maskParameters.y;
		  float normalMapStrength = maskParameters.z;
		  vec3 viewReflect = reflect(-vertexViewDir, ComputeNormal(tangentTransform, worldNormal, surfaceNormal, normalMapStrength));
		  float d = dot(viewReflect, maskAxis.xyz);
		  return clamp(1.0 - (d - fadeStart) / (fadeEnd - fadeStart), 0.0, 1.0);
	  }
	  else if (maskType == MASK_TYPE_FRESNEL) {
		  float power = maskParameters.x;
		  float fadeStart = maskParameters.y;
		  float fadeEnd = maskParameters.z;
		  float normalMapStrength = maskParameters.w;
		  float d = 1.0 - max(0.0, dot(vertexViewDir, ComputeNormal(tangentTransform, worldNormal, surfaceNormal, normalMapStrength)));
		  float p = pow(d, power);
		  return clamp(mix(fadeStart, fadeEnd, p), 0.0, 1.0);
	  }
	  else if (maskType == MASK_TYPE_PULSE) {
		  float distance = maskParameters.x;
		  float speed = maskParameters.y;
		  float power = maskParameters.z;
		  float d = dot(vertexObjPos, maskAxis.xyz);
		  float theta = 6.2831 * fract((d - elapsedSeconds * speed) / distance);
		  return clamp(pow((sin(theta) * 0.5 + 0.5), power), 0.0, 1.0);
	  }
	  else {
		  return 1.0;
	  }
	  return 1.0;
  }

  vec3 ComputeBlend(int blendMode, vec3 dst, vec3 src, float mask)
  {
	  if (blendMode == BLEND_MODE_MULTIPLY)
	  {
		  return dst * (src * mask);
	  }
	  else {
		  return dst + src * mask;
	  }
  }

  void main() {
	  vec3 worldNormal = normalize(vertexNormal);
	  mat3 tangentTransform = mat3(vertexTangent, vertexBitangent, worldNormal);

	  vec2 uv = vertexUV;
	  if (useProjector)
	  {
		  vec4 projectorPos = projectorInv * vec4(vertexWorldPos, 1.0);
		  if (abs(projectorPos.x) > 1.0 || abs(projectorPos.y) > 1.0 || abs(projectorPos.z) > 1.0)
		  {
			  discard;
			  return;
		  }
		  uv = projectorPos.xy * 0.5 + 0.5;
	  }

	  vec3 surfaceNormal = vec3(0.0, 0.0, 1.0);
	  if (useNormalMap)
	  {
		  surfaceNormal.xy = texture2D(normalMap, uv * normalMapScaleOffset.xy + normalMapScaleOffset.zw).xy * 2.0 - 1.0;
		  surfaceNormal.z = sqrt(1.0 - dot(surfaceNormal.xy, surfaceNormal.xy));
	  }

	  vec4 color = baseColor;
	  for (int i = 0; i < layerCount; ++i)
	  {
		  vec3 layerColor = ComputeColor(layerSamplerModes[i], uv, layerColors[i], layerSurfaces[i], layerSurfaceScaleOffsets[i], layerSampleParameters[i], tangentTransform, worldNormal, surfaceNormal);
		  float layerMask = ComputeMask(layerMaskTypes[i], layerMaskParameters[i], layerMaskAxes[i], tangentTransform, worldNormal, surfaceNormal);
		  color.rgb = ComputeBlend(layerBlendModes[i], color.rgb, layerColor, layerMask);
	  }

	  if (useAlpha)
	  {
		  color.a *= texture(alphaMask, uv * alphaMaskScaleOffset.xy + alphaMaskScaleOffset.zw).r;
	  }
	  color.a *= ComputeMask(baseMaskType, baseMaskParameters, baseMaskAxis, tangentTransform, worldNormal, surfaceNormal);
	  fragmentColor = color;
  })";

			GLuint id = glCreateProgram();
			if (id == 0){
				std::cerr << "Could not create shader program." << std::endl;
				return false;
			}
			GLuint f = 0, v = 0;

			GLuint s = glCreateShader(GL_VERTEX_SHADER);
			if (s == 0){
				fprintf(stderr, "Error:failed to create shader.\n");
				return 0;
			}
			// Pass shader source string
			const char *c = avatar_vertex_shader_string.c_str();
			glShaderSource(s, 1, &c, NULL);
			glCompileShader(s);
			v = s;
			if (v == 0){
				std::cerr << "vertex shader failed to compile." << std::endl;
				return false;
			}
			glAttachShader(id, v);

			GLuint s2 = glCreateShader(GL_FRAGMENT_SHADER);
			if (s2 == 0){
				fprintf(stderr, "Error:failed to create shader.\n");
				return 0;
			}
			// Pass shader source string
			const char *c2 = avatar_fragment_shader_string.c_str();
			glShaderSource(s2, 1, &c2, NULL);
			glCompileShader(s2);

			GLint ret;
			glGetShaderiv(s2, GL_COMPILE_STATUS, &ret);
			if (ret == false) {
				int infologLength = 0;
				glGetShaderiv(s, GL_INFO_LOG_LENGTH, &infologLength);
				GLchar buffer[128];
				GLsizei charsWritten = 0;
				glGetShaderInfoLog(id, infologLength, &charsWritten, buffer);
				std::cout << buffer << std::endl;
			}


			f = s2;
			if (f == 0){
				std::cerr << "Fragment shader failed to compile." << std::endl;
				return false;
			}
			glAttachShader(id, f);

			// Link program
			glLinkProgram(id);

			glGetProgramiv(id, GL_LINK_STATUS, &ret);
			if (ret == false) {
				int infologLength = 0;
				glGetProgramiv(id, GL_INFO_LOG_LENGTH, &infologLength);
				GLchar buffer[1209];
				GLsizei charsWritten = 0;
				glGetProgramInfoLog(id, infologLength, &charsWritten, buffer);
				std::cout << buffer << std::endl;
			}

			const auto & detach = [&id](const GLuint shader){
				if (shader){
					glDetachShader(id, shader);
					glDeleteShader(shader);
				}
			};
			detach(f);
			detach(v);

			return id;
		}

		IGL_INLINE bool OculusVR::init_VR_buffers(int window_width, int window_height) {
			for (int eye = 0; eye < 2; eye++) {
				eye_buffers[eye] = new OVR_buffer(session, eye);
				hand_buffers[eye] = new OVR_buffer(session, eye);
				ovrEyeRenderDesc& erd = eyeRenderDesc[eye] = ovr_GetRenderDesc(session, (ovrEyeType)eye, hmdDesc.DefaultEyeFov[eye]);
			}
			
			hud_buffer = new OVR_buffer(session);

			memset(&mirror_desc, 0, sizeof(mirror_desc));
			mirror_desc.Width = window_width;
			mirror_desc.Height = window_height;
			mirror_desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

ovr_CreateMirrorTextureWithOptionsGL(session, &mirror_desc, &_mirrorTexture);

// Configure the mirror read buffer
GLuint texId;
ovr_GetMirrorTextureBufferGL(session, _mirrorTexture, &texId);
glGenFramebuffers(1, &_mirrorFbo);
glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
{
	glDeleteFramebuffers(1, &_mirrorFbo);
	FAIL("Could not initialize VR buffers!");
	return false;
}

return true;
		}

		IGL_INLINE void	OculusVR::on_render_start() {
			eyeRenderDesc[0] = ovr_GetRenderDesc(session, (ovrEyeType)0, hmdDesc.DefaultEyeFov[0]);
			eyeRenderDesc[1] = ovr_GetRenderDesc(session, (ovrEyeType)1, hmdDesc.DefaultEyeFov[1]);
			// Get eye poses, feeding in correct IPD offset
			ovrPosef HmdToEyePose[2] = { eyeRenderDesc[0].HmdToEyePose, eyeRenderDesc[1].HmdToEyePose };
			// sensorSampleTime is fed into the layer later
			ovr_GetEyePoses(session, frame, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);
		}

		IGL_INLINE void OculusVR::handle_input(std::atomic<bool>& update_screen_while_computing, ViewerData& data) {
			ovr_GetSessionStatus(session, &sessionStatus);
			if (sessionStatus.ShouldRecenter) {
				request_recenter();
			}
			displayMidpointSeconds = ovr_GetPredictedDisplayTime(session, 0);
			hmdState = ovr_GetTrackingState(session, displayMidpointSeconds, ovrTrue);

			handPoses[ovrHand_Left] = hmdState.HandPoses[ovrHand_Left].ThePose;
			handPoses[ovrHand_Right] = hmdState.HandPoses[ovrHand_Right].ThePose;
			if (OVR_SUCCESS(ovr_GetInputState(session, ovrControllerType_Touch, &inputState))) {
				Eigen::Vector3f hand_pos;
				hand_pos << handPoses[ovrHand_Right].Position.x, handPoses[ovrHand_Right].Position.y, handPoses[ovrHand_Right].Position.z;

				if (inputState.Buttons & ovrButton_A) {
					count = (prev_press == A) ? count + 1 : 1;
					prev_press = A;
				}
				else if (inputState.Buttons & ovrButton_B) {
					count = (prev_press == B) ? count + 1 : 1;
					prev_press = B;
				}
				else if (inputState.HandTrigger[ovrHand_Right] <= 0.2f && inputState.IndexTrigger[ovrHand_Right] > 0.5f) {
					count = (prev_press == TRIG) ? count + 1 : 1;
					prev_press = TRIG;
				}
				else if (inputState.Thumbstick[ovrHand_Right].x > 0.1f || inputState.Thumbstick[ovrHand_Right].x < -0.1f || inputState.Thumbstick[ovrHand_Right].y > 0.1f || inputState.Thumbstick[ovrHand_Right].y < -0.1f) {
					navigate(inputState.Thumbstick[ovrHand_Right], data);
					count = 0;
				}
				else if (inputState.HandTrigger[ovrHand_Right] <= 0.2f && inputState.IndexTrigger[ovrHand_Right] <= 0.2f && !(inputState.Buttons & ovrButton_A) && !(inputState.Buttons & ovrButton_B) && !(inputState.Buttons & ovrButton_X) && !(inputState.Buttons & ovrButton_Y)) {
					count = (prev_press == NONE) ? count + 1 : 1;
					prev_press = NONE;
				}

				touch_dir_lock.lock();
				right_touch_direction = to_Eigen(OVR::Matrix4f(handPoses[ovrHand_Right].Orientation).Transform(OVR::Vector3f(0, 0, -1)));
				touch_dir_lock.unlock();

				if (menu_active) { //The menu is open, process input in a special way
					Eigen::Vector3f menu_center = to_Eigen((OVR::Vector3f)hmdState.HeadPose.ThePose.Position + ((OVR::Matrix4f)hmdState.HeadPose.ThePose.Orientation).Transform(OVR::Vector3f(0, 0, menu_z_pos)));
					set_menu_3D_mouse(hand_pos, get_right_touch_direction(), menu_center, hud_buffer->eyeTextureSize.w*pixels_to_meter, hud_buffer->eyeTextureSize.h*pixels_to_meter);
					if (prev_press == TRIG || prev_press == B) {
						callback_GUI_button_press(); //Click in GUI menu
						//callback_menu_closed();
						return;
					}
					else if (prev_press == A) { //Click to close GUI
						std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
						std::chrono::duration<float> deltaTime = currentTime - menu_lastTime;
						if (deltaTime.count() > 1) { //Block menu input for 1 sec
							callback_menu_closed();
							menu_active = false;
							menu_lastTime = std::chrono::steady_clock::now();
							count = 1;
						}
						return;
					}
				}
				else if (!menu_active && (prev_press == A)) { //Bring up the menu
					std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
					std::chrono::duration<float> deltaTime = currentTime - menu_lastTime;
					if (deltaTime.count() > 1) {//Block menu input for 1 sec 
						//TODO: pop-up the menu
						callback_menu_opened();
						menu_active = true;
						menu_lastTime = std::chrono::steady_clock::now();
						count = 1;
					}
					//TODO: check whether to return or proceed to below
					return;
				}
				else if (!menu_active && (prev_press == B)) {
					count = 1;
					return;
				}

				/*else if ((prev_press == MENU) && menu_active) {
					std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
					std::chrono::duration<float> deltaTime = currentTime - menu_lastTime;
					if (deltaTime.count() > 1) { //Block menu input for 1 sec
						callback_menu_closed();
						menu_active = false;
						menu_lastTime = std::chrono::steady_clock::now();
						count = 1;
					}
					return;
				}*/

				//Only TRIG and NONE can come here with count >=3
				if (count >= 3) { //Only do a callback when we have at least 3 of the same buttonCombos in a row (to prevent doing a GRIP/TRIG action when we were actually starting up a GRIPTRIG)
					if ((prev_press == NONE && count == 3 && prev_sent != NONE)) {
						update_screen_while_computing = true;
						std::thread t1(callback_button_down, prev_press, hand_pos);
						t1.detach();
					}
					else {
						callback_button_down(prev_press, hand_pos);
					}

					count = 3; //Avoid overflow
					prev_sent = prev_press;
				}
			}
		}

		IGL_INLINE void OculusVR::handle_avatar_messages(igl::opengl::glfw::Viewer* viewer) {
			lastTime = std::chrono::steady_clock::now();
			while (ovrAvatarMessage* message = ovrAvatarMessage_Pop()) {
				switch (ovrAvatarMessage_GetType(message)) {
				case ovrAvatarMessageType_AvatarSpecification:
					handle_avatar_specification(ovrAvatarMessage_GetAvatarSpecification(message));
					break;
				case ovrAvatarMessageType_AssetLoaded:
					handle_asset_loaded(ovrAvatarMessage_GetAssetLoaded(message), viewer);
					break;
				}
				ovrAvatarMessage_Free(message);
			}
		}

		IGL_INLINE void OculusVR::draw(std::vector<ViewerData>& data_list, glfw::ViewerPlugin* gui, GLFWwindow* window, ViewerCore& core, std::atomic<bool>& update_screen_while_computing) {
			do {
				/*// Keyboard inputs to adjust player orientation
				static float Yaw(0.0f);

				if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  Yaw += 0.02f;
				if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) Yaw -= 0.02f;

				// Keyboard inputs to adjust player position
				static OVR::Vector3f Pos2(0.0f, 0.0f, 0.0f);
				if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)   Pos2 += OVR::Matrix4f::RotationY(Yaw).Transform(OVR::Vector3f(0, 0, -0.05f));
				if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)   Pos2 += OVR::Matrix4f::RotationY(Yaw).Transform(OVR::Vector3f(0, 0, +0.05f));
				if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)   Pos2 += OVR::Matrix4f::RotationY(Yaw).Transform(OVR::Vector3f(+0.05f, 0, 0));
				if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)   Pos2 += OVR::Matrix4f::RotationY(Yaw).Transform(OVR::Vector3f(-0.05f, 0, 0));*/

				// Compute how much time has elapsed since the last frame
				std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
				std::chrono::duration<float> deltaTime = currentTime - lastTime;
				float deltaSeconds = deltaTime.count();
				lastTime = currentTime;
				_elapsedSeconds += deltaSeconds;

				on_render_start();
				if (_avatar) {
					update_avatar(deltaSeconds);
				}
				GLenum err;
				err = glGetError();

				Eigen::Matrix4f proj, view;
				for (int eye = 0; eye < 2; eye++)
				{
					OVR::Matrix4f finalRollPitchYaw = OVR::Matrix4f(EyeRenderPose[eye].Orientation);
					OVR::Vector3f finalUp = finalRollPitchYaw.Transform(OVR::Vector3f(0, 1, 0));
					OVR::Vector3f finalForward = finalRollPitchYaw.Transform(OVR::Vector3f(0, 0, -1));
					OVR::Vector3f shiftedEyePos = OVR::Vector3f(EyeRenderPose[eye].Position);

					view = to_Eigen(OVR::Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp)); //OVRLookATRH gives exact same result as igl::look_at with same input
					OVR::Matrix4f proj_tmp = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], 0.01f, 1000.0f, (ovrProjection_ClipRangeOpenGL));
					proj = to_Eigen(proj_tmp);

					eye_buffers[eye]->OnRender();
					for (int i = 0; i < data_list.size(); i++) {
						core.draw(data_list[i], true, true, view, proj);
					}
					eye_buffers[eye]->OnRenderFinish();


					GLfloat prev_clear_color[4];
					glGetFloatv(GL_COLOR_CLEAR_VALUE, prev_clear_color);
					glClearColor(0, 0, 0, 0);
					hand_buffers[eye]->OnRender();
					if (_avatar && !_loadingAssets && !_waitingOnCombinedMesh) {
						render_avatar(_avatar, ovrAvatarVisibilityFlag_FirstPerson, view, proj, to_Eigen(shiftedEyePos), false);
					}
					hand_buffers[eye]->OnRenderFinish();
					glClearColor(prev_clear_color[0], prev_clear_color[1], prev_clear_color[2], prev_clear_color[3]);


					ovr_CommitTextureSwapChain(session, eye_buffers[eye]->swapTextureChain);
					ovr_CommitTextureSwapChain(session, hand_buffers[eye]->swapTextureChain);

				}

				if (((igl::opengl::glfw::imgui::ImGuiMenu*)gui)->is_active()) {
					hud_buffer->OnRenderHud();
					gui->pre_draw();
					gui->post_draw();
					hud_buffer->OnRenderFinishHud();

					ovr_CommitTextureSwapChain(session, hud_buffer->swapTextureChain);
				}
				submit_frame();
				blit_mirror();

				// Swap buffers
				glfwSwapBuffers(window);
				glfwPollEvents();
			} while (update_screen_while_computing);
		}

		IGL_INLINE OculusVR::OVR_buffer::OVR_buffer(const ovrSession &session, int eyeIdx) {
			eyeTextureSize = ovr_GetFovTextureSize(session, (ovrEyeType)eyeIdx, hmdDesc.DefaultEyeFov[eyeIdx], 1.0f);

			ovrTextureSwapChainDesc desc = {};
			desc.Type = ovrTexture_2D;
			desc.ArraySize = 1;
			desc.Width = eyeTextureSize.w;
			desc.Height = eyeTextureSize.h;
			desc.MipLevels = 1;
			desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
			desc.SampleCount = 1;
			desc.StaticImage = ovrFalse;

			ovrResult result = ovr_CreateTextureSwapChainGL(session, &desc, &swapTextureChain);

			int textureCount = 0;
			ovr_GetTextureSwapChainLength(session, swapTextureChain, &textureCount);

			for (int j = 0; j < textureCount; ++j)
			{
				GLuint chainTexId;
				ovr_GetTextureSwapChainBufferGL(session, swapTextureChain, j, &chainTexId);
				glBindTexture(GL_TEXTURE_2D, chainTexId);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			//Create eye buffer to render to
			glGenFramebuffers(1, &eyeFbo);
			
			// create depth buffer
			glGenTextures(1, &depthBuffer);
			glBindTexture(GL_TEXTURE_2D, depthBuffer);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, eyeTextureSize.w, eyeTextureSize.h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
		}

		IGL_INLINE OculusVR::OVR_buffer::OVR_buffer(const ovrSession &session) {
			ovrTextureSwapChainDesc desc = {};
			desc.Type = ovrTexture_2D;
			desc.ArraySize = 1;
			desc.Width = 512;
			desc.Height = 512;
			desc.MipLevels = 1;
			desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
			desc.SampleCount = 1;
			desc.StaticImage = ovrFalse;

			eyeTextureSize.w = desc.Width;
			eyeTextureSize.h = desc.Height;

			ovrResult result = ovr_CreateTextureSwapChainGL(session, &desc, &swapTextureChain);

				int textureCount = 0;
			ovr_GetTextureSwapChainLength(session, swapTextureChain, &textureCount);

			for (int j = 0; j < textureCount; ++j)
			{
				GLuint chainTexId;
				ovr_GetTextureSwapChainBufferGL(session, swapTextureChain, j, &chainTexId);
				glBindTexture(GL_TEXTURE_2D, chainTexId);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			//Create eye buffer to render to
			glGenFramebuffers(1, &eyeFbo);

			// create depth buffer
		/*	glGenTextures(1, &depthBuffer);
			glBindTexture(GL_TEXTURE_2D, depthBuffer);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 512, 512, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);*/
		}

		IGL_INLINE void OculusVR::OVR_buffer::OnRender() {
			int currentIndex;
			ovr_GetTextureSwapChainCurrentIndex(session, swapTextureChain, &currentIndex);
			ovr_GetTextureSwapChainBufferGL(session, swapTextureChain, currentIndex, &eyeTexId);

			// Switch to eye render target
			glBindFramebuffer(GL_FRAMEBUFFER, eyeFbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTexId, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffer, 0);
			GLenum err = glGetError();
			glViewport(0, 0, eyeTextureSize.w, eyeTextureSize.h);
			err = glGetError();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_FRAMEBUFFER_SRGB);
		}

		IGL_INLINE void OculusVR::OVR_buffer::OnRenderFinish() {
			GLenum err = glGetError();
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			err = glGetError();
		//	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			err = glGetError();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			err = glGetError();

		}

		IGL_INLINE void OculusVR::OVR_buffer::OnRenderHud() {
			int currentIndex;
			ovr_GetTextureSwapChainCurrentIndex(session, swapTextureChain, &currentIndex);
			ovr_GetTextureSwapChainBufferGL(session, swapTextureChain, currentIndex, &eyeTexId);

			// Switch to eye render target
			glBindFramebuffer(GL_FRAMEBUFFER, eyeFbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTexId, 0);
			glViewport(0, 0, eyeTextureSize.w, eyeTextureSize.h);
			glClear(GL_COLOR_BUFFER_BIT);
			glEnable(GL_FRAMEBUFFER_SRGB);
		}

		IGL_INLINE void OculusVR::OVR_buffer::OnRenderFinishHud() {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

		}

		IGL_INLINE void OculusVR::submit_frame() {
			// create the main eye layer
			ovrLayerEyeFov eyeLayer, handLayer;
			eyeLayer.Header.Type = ovrLayerType_EyeFov;
			eyeLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.
			handLayer.Header.Type = ovrLayerType_EyeFov;
			handLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;

			for (int eye = 0; eye < 2; eye++)
			{
				eyeLayer.ColorTexture[eye] = eye_buffers[eye]->swapTextureChain;
				eyeLayer.Viewport[eye] = OVR::Recti(eye_buffers[eye]->eyeTextureSize);
				eyeLayer.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
				eyeLayer.RenderPose[eye] = EyeRenderPose[eye];
				eyeLayer.SensorSampleTime = sensorSampleTime;

				handLayer.ColorTexture[eye] = hand_buffers[eye]->swapTextureChain;
				handLayer.Viewport[eye] = OVR::Recti(hand_buffers[eye]->eyeTextureSize);
				handLayer.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
				handLayer.RenderPose[eye] = EyeRenderPose[eye];
				handLayer.SensorSampleTime = sensorSampleTime;

			}


			// Create HUD layer, fixed to the player's torso
			ovrLayerQuad hudLayer;
			hudLayer.Header.Type = ovrLayerType_Quad;
			hudLayer.Header.Flags =  ovrLayerFlag_HighQuality | ovrLayerFlag_TextureOriginAtBottomLeft | ovrLayerFlag_HeadLocked;
			hudLayer.ColorTexture = hud_buffer->swapTextureChain;

			hudLayer.QuadPoseCenter.Position.x = 0.00f;
			hudLayer.QuadPoseCenter.Position.y = 0.00f;
			hudLayer.QuadPoseCenter.Position.z = menu_z_pos;
			hudLayer.QuadPoseCenter.Orientation.x = 0;
			hudLayer.QuadPoseCenter.Orientation.y = 0;
			hudLayer.QuadPoseCenter.Orientation.z = 0;
			hudLayer.QuadPoseCenter.Orientation.w = 1;
			// HUD is 50cm wide, 30cm tall.
			//hudLayer.QuadSize.x = 0.50f;
			//hudLayer.QuadSize.y = 0.30f;
			hudLayer.QuadSize = OVR::Vector2f((float)hud_buffer->eyeTextureSize.w, (float)hud_buffer->eyeTextureSize.h) * pixels_to_meter;
			// Display all of the HUD texture.
			hudLayer.Viewport.Pos.x = 0.0f;
			hudLayer.Viewport.Pos.y = 0.0f;
			hudLayer.Viewport.Size.w = hud_buffer->eyeTextureSize.w;
			hudLayer.Viewport.Size.h = hud_buffer->eyeTextureSize.h;

			ovrViewScaleDesc viewScaleDesc;
			viewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
			viewScaleDesc.HmdToEyePose[0] = eyeRenderDesc[0].HmdToEyePose;
			viewScaleDesc.HmdToEyePose[1] = eyeRenderDesc[1].HmdToEyePose;

			if (menu_active) {
				ovrLayerHeader* layerList[3];
				layerList[0] = &eyeLayer.Header;
				layerList[1] = &hudLayer.Header;
				layerList[2] = &handLayer.Header;

				ovrResult result = ovr_SubmitFrame(session, frame, &viewScaleDesc, layerList, 3);
			}
			else {
				ovrLayerHeader* layerList[2];
				layerList[0] = &eyeLayer.Header;
				layerList[1] = &handLayer.Header;

				ovrResult result = ovr_SubmitFrame(session, frame, &viewScaleDesc, layerList, 2);
			}

		}

		IGL_INLINE void OculusVR::blit_mirror() {
			// Blit mirror texture to back buffer
			glBindFramebuffer(GL_READ_FRAMEBUFFER, _mirrorFbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			GLint w = mirror_desc.Width;
			GLint h = mirror_desc.Height;

			glBlitFramebuffer(0, h, w, 0, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		}

		IGL_INLINE void OculusVR::update_avatar(float deltaSeconds) {
			// Convert the OVR inputs into Avatar SDK inputs
			ovrInputState touchState;
			ovr_GetInputState(session, ovrControllerType_Active, &touchState);
			ovrTrackingState trackingState = ovr_GetTrackingState(session, 0.0, false);

			ovrAvatarTransform hmd;
			ovrAvatarTransform_from_OVR(trackingState.HeadPose.ThePose.Position, trackingState.HeadPose.ThePose.Orientation, &hmd);

			ovrAvatarTransform left;
			ovrAvatarTransform_from_OVR(trackingState.HandPoses[ovrHand_Left].ThePose.Position, trackingState.HandPoses[ovrHand_Left].ThePose.Orientation, &left);

			ovrAvatarTransform right;
			ovrAvatarTransform_from_OVR(trackingState.HandPoses[ovrHand_Right].ThePose.Position, trackingState.HandPoses[ovrHand_Right].ThePose.Orientation, &right);

			ovrAvatarHandInputState inputStateLeft;
			ovrAvatarHandInputState_from_OVR(left, touchState, ovrHand_Left, &inputStateLeft);

			ovrAvatarHandInputState inputStateRight;
			ovrAvatarHandInputState_from_OVR(right, touchState, ovrHand_Right, &inputStateRight);

			// Update the avatar pose from the inputs
			ovrAvatarPose_UpdateBody(_avatar, hmd);
			ovrAvatarPose_UpdateHands(_avatar, inputStateLeft, inputStateRight);
			ovrAvatarPose_Finalize(_avatar, deltaSeconds); //TODO: might need to set deltaseconds here
		}

		IGL_INLINE void OculusVR::ovrAvatarTransform_from_OVR(const ovrVector3f& position, const ovrQuatf& orientation, ovrAvatarTransform* target) {
			target->position.x = position.x;
			target->position.y = position.y;
			target->position.z = position.z;
			target->orientation.x = orientation.x;
			target->orientation.y = orientation.y;
			target->orientation.z = orientation.z;
			target->orientation.w = orientation.w;
			target->scale.x = 1.0f; //Scale is fixed at 1
			target->scale.y = 1.0f;
			target->scale.z = 1.0f;
		}

		IGL_INLINE void OculusVR::ovrAvatarHandInputState_from_OVR(const ovrAvatarTransform& transform, const ovrInputState& inputState, ovrHandType hand, ovrAvatarHandInputState* state)
		{
			state->transform = transform;
			state->buttonMask = 0;
			state->touchMask = 0;
			state->joystickX = inputState.Thumbstick[hand].x;
			state->joystickY = inputState.Thumbstick[hand].y;
			state->indexTrigger = inputState.IndexTrigger[hand];
			state->handTrigger = inputState.HandTrigger[hand];
			state->isActive = false;
			if (hand == ovrHand_Left)
			{
				if (inputState.Buttons & ovrButton_X) state->buttonMask |= ovrAvatarButton_One;
				if (inputState.Buttons & ovrButton_Y) state->buttonMask |= ovrAvatarButton_Two;
				if (inputState.Buttons & ovrButton_Enter) state->buttonMask |= ovrAvatarButton_Three;
				if (inputState.Buttons & ovrButton_LThumb) state->buttonMask |= ovrAvatarButton_Joystick;
				if (inputState.Touches & ovrTouch_X) state->touchMask |= ovrAvatarTouch_One;
				if (inputState.Touches & ovrTouch_Y) state->touchMask |= ovrAvatarTouch_Two;
				if (inputState.Touches & ovrTouch_LThumb) state->touchMask |= ovrAvatarTouch_Joystick;
				if (inputState.Touches & ovrTouch_LThumbRest) state->touchMask |= ovrAvatarTouch_ThumbRest;
				if (inputState.Touches & ovrTouch_LIndexTrigger) state->touchMask |= ovrAvatarTouch_Index;
				if (inputState.Touches & ovrTouch_LIndexPointing) state->touchMask |= ovrAvatarTouch_Pointing;
				if (inputState.Touches & ovrTouch_LThumbUp) state->touchMask |= ovrAvatarTouch_ThumbUp;
				state->isActive = (inputState.ControllerType & ovrControllerType_LTouch) != 0;
			}
			else if (hand == ovrHand_Right)
			{
				if (inputState.Buttons & ovrButton_A) state->buttonMask |= ovrAvatarButton_One;
				if (inputState.Buttons & ovrButton_B) state->buttonMask |= ovrAvatarButton_Two;
				if (inputState.Buttons & ovrButton_Home) state->buttonMask |= ovrAvatarButton_Three;
				if (inputState.Buttons & ovrButton_RThumb) state->buttonMask |= ovrAvatarButton_Joystick;
				if (inputState.Touches & ovrTouch_A) state->touchMask |= ovrAvatarTouch_One;
				if (inputState.Touches & ovrTouch_B) state->touchMask |= ovrAvatarTouch_Two;
				if (inputState.Touches & ovrTouch_RThumb) state->touchMask |= ovrAvatarTouch_Joystick;
				if (inputState.Touches & ovrTouch_RThumbRest) state->touchMask |= ovrAvatarTouch_ThumbRest;
				if (inputState.Touches & ovrTouch_RIndexTrigger) state->touchMask |= ovrAvatarTouch_Index;
				if (inputState.Touches & ovrTouch_RIndexPointing) state->touchMask |= ovrAvatarTouch_Pointing;
				if (inputState.Touches & ovrTouch_RThumbUp) state->touchMask |= ovrAvatarTouch_ThumbUp;
				state->isActive = (inputState.ControllerType & ovrControllerType_RTouch) != 0;
			}
		}

		IGL_INLINE void OculusVR::handle_avatar_specification(const ovrAvatarMessage_AvatarSpecification* message) {
			// Create the avatar instance
			_avatar = ovrAvatar_Create(message->avatarSpec, ovrAvatarCapability_Hands);
			ovrAvatar_SetLeftControllerVisibility(_avatar, 0);
			ovrAvatar_SetRightControllerVisibility(_avatar, 0);
			// Trigger load operations for all of the assets referenced by the avatar
			uint32_t refCount = ovrAvatar_GetReferencedAssetCount(_avatar);
			for (uint32_t i = 0; i < refCount; ++i) {
				ovrAvatarAssetID id = ovrAvatar_GetReferencedAsset(_avatar, i);
				ovrAvatarAsset_BeginLoading(id);
				++_loadingAssets;
			}
			printf("Loading %d assets...\r\n", _loadingAssets);
		}

		IGL_INLINE void OculusVR::handle_asset_loaded(const ovrAvatarMessage_AssetLoaded* message, igl::opengl::glfw::Viewer* viewer) {
			// Determine the type of the asset that got loaded
			ovrAvatarAssetType assetType = ovrAvatarAsset_GetType(message->asset);
			void* data = nullptr;

			// Call the appropriate loader function
			switch (assetType) {
			case ovrAvatarAssetType_Mesh:
				std::cout << "Mesh" << std::endl;
				data = loadMesh(ovrAvatarAsset_GetMeshData(message->asset));
				break;
			case ovrAvatarAssetType_Texture:
				std::cout << "texture" << std::endl;

				data = loadTexture(ovrAvatarAsset_GetTextureData(message->asset));
				break;
			case ovrAvatarAssetType_CombinedMesh:
				std::cout << "CombinedMesh" << std::endl;
				//data = _loadCombinedMesh(ovrAvatarAsset_GetCombinedMeshData(message->asset));
				break;
			default:
				break;
			}

			// Store the data that we loaded for the asset in the asset map
			_assetMap[message->assetID] = data;

			if (assetType == ovrAvatarAssetType_CombinedMesh) {
				std::cout << "unexpectedly encountered a CombinedMesh type. These do not work yet. " << std::endl;
				//	uint32_t idCount = 0;
					//ovrAvatarAsset_GetCombinedMeshIDs(message->asset, &idCount);
					//_loadingAssets -= idCount;
					//ovrAvatar_GetCombinedMeshAlphaData(_avatar, &_avatarCombinedMeshAlpha, &_avatarCombinedMeshAlphaOffset);
			}
			else {
				--_loadingAssets;
			}

			printf("Loading %d assets...\r\n", _loadingAssets);
		}

		IGL_INLINE OculusVR::AvatarMeshData* OculusVR::loadMesh(const ovrAvatarMeshAssetData* data) {
			AvatarMeshData* mesh = new AvatarMeshData();

			// Create the vertex array and buffer
			glGenVertexArrays(1, &mesh->vertexArray);
			glGenBuffers(1, &mesh->vertexBuffer);
			glGenBuffers(1, &mesh->elementBuffer);

			// Bind the vertex buffer and assign the vertex data	
			glBindVertexArray(mesh->vertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, mesh->vertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, data->vertexCount * sizeof(ovrAvatarMeshVertex), data->vertexBuffer, GL_STATIC_DRAW);

			// Bind the index buffer and assign the index data
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->elementBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, data->indexCount * sizeof(GLushort), data->indexBuffer, GL_STATIC_DRAW);
			mesh->elementCount = data->indexCount;

			// Fill in the array attributes
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->x);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->nx);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->tx);
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->u);
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(4, 4, GL_BYTE, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->blendIndices);
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(ovrAvatarMeshVertex), &((ovrAvatarMeshVertex*)0)->blendWeights);
			glEnableVertexAttribArray(5);

			// Clean up
			glBindVertexArray(0);

			// Translate the bind pose
			_computeWorldPose(data->skinnedBindPose, mesh->bindPose);
			for (uint32_t i = 0; i < data->skinnedBindPose.jointCount; ++i)
			{
				mesh->inverseBindPose[i] = mesh->bindPose[i].inverse();
			}
			return mesh;
		}

		IGL_INLINE void OculusVR::set_menu_3D_mouse(Eigen::Vector3f& start, Eigen::Vector3f& dir, Eigen::Vector3f& menu_center, float menu_width, float menu_height) {
			
			Eigen::Vector3d p0(menu_center[0] + 0.5*menu_width, menu_center[1] + 0.5*menu_height, menu_center[2]);
			Eigen::Vector3d p1(menu_center[0] + 0.5*menu_width, menu_center[1] - 0.5*menu_height, menu_center[2]);
			Eigen::Vector3d p2(menu_center[0] - 0.5*menu_width, menu_center[1] + 0.5*menu_height, menu_center[2]);
			Eigen::Vector3d p3(menu_center[0] - 0.5*menu_width, menu_center[1] - 0.5*menu_height, menu_center[2]);

			Eigen::Vector3d ray_start = start.cast<double>();
			Eigen::Vector3d ray_dir = dir.cast<double>();

			float menu_width_pixels = menu_width/pixels_to_meter;
			float menu_height_pixels = menu_height/pixels_to_meter;

			double t, u, v;
			Eigen::Vector3d pt;
			if (intersect_triangle(ray_start.data(), ray_dir.data(), p0.data(), p1.data(), p2.data(), &t, &u, &v) && t>0) {
				pt = ray_start + t*ray_dir;
				pt -= menu_center.cast<double>();
				Eigen::Vector2f mouse_pos = Eigen::Vector2f(menu_width_pixels*((pt[0]-(-menu_width*0.5f))/menu_width), menu_height_pixels - menu_height_pixels*((pt[1] -(-menu_height*0.5f))/menu_height));
				callback_GUI_set_mouse(mouse_pos);
			}
			else if (intersect_triangle(ray_start.data(), ray_dir.data(), p3.data(), p1.data(), p2.data(), &t, &u, &v) > 0) {
				pt = ray_start + t*ray_dir;
				pt -= menu_center.cast<double>();
				Eigen::Vector2f mouse_pos = Eigen::Vector2f(menu_width_pixels*((pt[0] - (-menu_width*0.5f)) / menu_width), menu_height_pixels - menu_height_pixels*((pt[1] - (-menu_height*0.5f)) / menu_height));
				callback_GUI_set_mouse(mouse_pos);
			}
		}

		IGL_INLINE void OculusVR::hapticPulse() {
			//std::chrono::steady_clock::time_point time_start = std::chrono::steady_clock::now();
			//std::chrono::duration<float> deltaTime;
			//do {
				//ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 1.0f);
			//	deltaTime = std::chrono::steady_clock::now() - time_start;
			//} while (deltaTime.count() < 1000);
			//ovr_SetControllerVibration(session, ovrControllerType_RTouch, 0.0f, 0.0f);

				ovrHapticsPlaybackState playbackState;
				ovrResult result = ovr_GetControllerVibrationState(session, ovrControllerType_RTouch, &playbackState);
				if (playbackState.RemainingQueueSpace >= hapticBufferSize)
				{
					ovr_SubmitControllerVibration(session, ovrControllerType_RTouch, &_hapticBuffer);
				}
		}
	
		IGL_INLINE void OculusVR::set_menu_hover_callback() {
//			igl::opengl::glfw::imgui::ImGuiMenu::callback_menu_hover = hapticPulse;
		}

		IGL_INLINE void OculusVR::_computeWorldPose(const ovrAvatarSkinnedMeshPose& localPose, Eigen::Matrix4f* worldPose){
			for (uint32_t i = 0; i < localPose.jointCount; ++i)
			{
				Eigen::Matrix4f local;
				EigenFromOvrAvatarTransform(localPose.jointTransform[i], local);
				//std::cout << localPose.jointParents[i] <<  " local pose: " << localPose.jointNames[i] << "   " << localPose.jointTransform[i].position.x << "   " << localPose.jointTransform[i].position.y << "   " << localPose.jointTransform[i].position.z << std::endl;
				int parentIndex = localPose.jointParents[i];
				if (parentIndex < 0)
				{
					*(worldPose + i) = local;
				}
				else
				{
					*(worldPose + i) = *(worldPose + parentIndex) * local;
				}
			}
		}

		IGL_INLINE void OculusVR::EigenFromOvrAvatarTransform(const ovrAvatarTransform& transform, Eigen::Matrix4f& target) {
			Eigen::Matrix4f scale = Eigen::Matrix4f::Identity();
			scale.topLeftCorner(3, 3) = Scaling(transform.scale.x, transform.scale.y, transform.scale.z);
			Eigen::Matrix4f translation = Eigen::Matrix4f::Identity();
			translation.col(3) << transform.position.x, transform.position.y, transform.position.z, 1.0f;
			Eigen::Matrix4f rot = Eigen::Matrix4f::Identity();
			rot.topLeftCorner(3, 3) = Eigen::Quaternionf(transform.orientation.w, transform.orientation.x, transform.orientation.y, transform.orientation.z).matrix();
			target = translation * rot * scale;
		}

		IGL_INLINE OculusVR::AvatarTextureData* OculusVR::loadTexture(const ovrAvatarTextureAssetData* data) {
			// Create a texture
			AvatarTextureData* texture = new AvatarTextureData();
			glGenTextures(1, &texture->textureID);
			glBindTexture(GL_TEXTURE_2D, texture->textureID);

			// Load the image data
			switch (data->format)
			{
				// Handle uncompressed image data
			case ovrAvatarTextureFormat_RGB24:
				for (uint32_t level = 0, offset = 0, width = data->sizeX, height = data->sizeY; level < data->mipCount; ++level)
				{
					glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data->textureData + offset);
					offset += width * height * 3;
					width /= 2;
					height /= 2;
				}
				break;

				// Handle compressed image data
			case ovrAvatarTextureFormat_DXT1:
			case ovrAvatarTextureFormat_DXT5:
			{
				GLenum glFormat;
				int blockSize;
				if (data->format == ovrAvatarTextureFormat_DXT1)
				{
					blockSize = 8;
					glFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				}
				else
				{
					blockSize = 16;
					glFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				}

				for (uint32_t level = 0, offset = 0, width = data->sizeX, height = data->sizeY; level < data->mipCount; ++level)
				{
					GLsizei levelSize = (width < 4 || height < 4) ? blockSize : blockSize * (width / 4) * (height / 4);
					glCompressedTexImage2D(GL_TEXTURE_2D, level, glFormat, width, height, 0, levelSize, data->textureData + offset);
					offset += levelSize;
					width /= 2;
					height /= 2;
				}
				break;
			}

			// Handle ASTC data
			case ovrAvatarTextureFormat_ASTC_RGB_6x6_MIPMAPS:
			{
				const unsigned char * level = (const unsigned char*)data->textureData;

				unsigned int w = data->sizeX;
				unsigned int h = data->sizeY;
				for (unsigned int i = 0; i < data->mipCount; i++)
				{
					int32_t blocksWide = (w + 5) / 6;
					int32_t blocksHigh = (h + 5) / 6;
					int32_t mipSize = 16 * blocksWide * blocksHigh;

					//glCompressedTexImage2D(GL_TEXTURE_2D, i, GL_COMPRESSED_RGBA_ASTC_6x6_KHR, w, h, 0, mipSize, level);

					level += mipSize;

					w >>= 1;
					h >>= 1;
					if (w < 1) { w = 1; }
					if (h < 1) { h = 1; }
				}
				break;
			}

			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			return texture;
		}

		IGL_INLINE void OculusVR::render_avatar(ovrAvatar* avatar, uint32_t visibilityMask, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints) {
			// Traverse over all components on the avatar
			uint32_t componentCount = ovrAvatarComponent_Count(avatar);
			const ovrAvatarComponent* bodyComponent = nullptr;
			if (const ovrAvatarBodyComponent* body = ovrAvatarPose_GetBodyComponent(avatar))
			{
				bodyComponent = body->renderComponent;
			}

			for (uint32_t i = 0; i < componentCount; ++i)
			{
				const ovrAvatarComponent* component = ovrAvatarComponent_Get(avatar, i);
				const bool useCombinedMeshProgram = _combineMeshes && bodyComponent == component;
		//		std::cout << (component->name) << std::endl;
				// Compute the transform for this component
				Eigen::Matrix4f world;
				EigenFromOvrAvatarTransform(component->transform, world);
				// Render each render part attached to the component
				for (uint32_t j = 0; j < component->renderPartCount; ++j)
				{
					const ovrAvatarRenderPart* renderPart = component->renderParts[j];
					ovrAvatarRenderPartType type = ovrAvatarRenderPart_GetType(renderPart);
					switch (type)
					{
					case ovrAvatarRenderPartType_SkinnedMeshRender:
						//For hands
						_renderSkinnedMeshPart(ovrAvatarRenderPart_GetSkinnedMeshRender(renderPart), visibilityMask, world, view, proj, viewPos, renderJoints);
						break;
					case ovrAvatarRenderPartType_SkinnedMeshRenderPBS:
						//For controllers
						//		_renderSkinnedMeshPartPBS(ovrAvatarRenderPart_GetSkinnedMeshRenderPBS(renderPart), visibilityMask, world, view, proj, viewPos, renderJoints);
						break;
					case ovrAvatarRenderPartType_ProjectorRender:
						//Unknown

						//	_renderProjector(ovrAvatarRenderPart_GetProjectorRender(renderPart), avatar, visibilityMask, world, view, proj, viewPos);
						break;
					}
				}
			}
		}

		IGL_INLINE void OculusVR::_renderSkinnedMeshPart(const ovrAvatarRenderPart_SkinnedMeshRender* mesh, uint32_t visibilityMask, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints)
		{
			// If this part isn't visible from the viewpoint we're rendering from, do nothing
			if ((mesh->visibilityMask & visibilityMask) == 0)
			{
				//std::cout << "returning" << std::endl;
				return;
			}

			AvatarMeshData* data = (AvatarMeshData*)_assetMap[mesh->meshAssetID];
		
			glUseProgram((GLuint)_skinnedMeshProgram);
			
			// Apply the vertex state
			_setMeshState(_skinnedMeshProgram, mesh->localTransform, data, mesh->skinnedPose, world, view, proj, viewPos);

			// Apply the material state
			_setMaterialState(_skinnedMeshProgram, &mesh->materialState, nullptr);

			// Draw the mesh
			glBindVertexArray(data->vertexArray);
			GLfloat cur_depth_func, cur_depth_mask;
			glGetFloatv(GL_DEPTH_FUNC, &cur_depth_func);
			glGetFloatv(GL_DEPTH_WRITEMASK, &cur_depth_mask);
			glDepthFunc(GL_LESS);

			// Write to depth first for self-occlusion
			if (mesh->visibilityMask & ovrAvatarVisibilityFlag_SelfOccluding)
			{
					glDepthMask(GL_TRUE);
					glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

					glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
					glDepthFunc(GL_EQUAL);
			}

			// Render to color buffer
			glDepthMask(GL_FALSE);
			glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDrawElements(GL_TRIANGLES, (GLsizei)data->elementCount, GL_UNSIGNED_SHORT, 0);
			glBindVertexArray(0);

			glDepthFunc(cur_depth_func);
			glDepthMask(cur_depth_mask);

			/*if (renderJoints)
			{
				Eigen::Matrix4f local;
				EigenFromOvrAvatarTransform(mesh->localTransform, local);
				glDepthFunc(GL_ALWAYS);
				_renderPose(proj * view * world * local, mesh->skinnedPose);
			}*/
		}

		IGL_INLINE void OculusVR::_setMeshState(GLuint program, const ovrAvatarTransform& localTransform, const AvatarMeshData* data, const ovrAvatarSkinnedMeshPose& skinnedPose, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f proj, const Eigen::Vector3f& viewPos) {
			// Compute the final world and viewProjection matrices for this part
			Eigen::Matrix4f local;
			EigenFromOvrAvatarTransform(localTransform, local);
			Eigen::Matrix4f worldMat = world * local;
			Eigen::Matrix4f viewProjMat = proj * view;
			// Compute the skinned pose
			Eigen::Matrix4f* skinnedPoses = (Eigen::Matrix4f*)alloca(sizeof(Eigen::Matrix4f) * skinnedPose.jointCount);
			_computeWorldPose(skinnedPose, skinnedPoses);
			for (uint32_t i = 0; i < skinnedPose.jointCount; ++i)
			{
				*(skinnedPoses+i) = *(skinnedPoses+i) * data->inverseBindPose[i];
			}

			// Pass the world view position to the shader for view-dependent rendering
			glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, viewPos.data());

			// Assign the vertex uniforms
			glUniformMatrix4fv(glGetUniformLocation(program, "world"), 1, 0, worldMat.data());
			glUniformMatrix4fv(glGetUniformLocation(program, "viewProj"), 1, 0, viewProjMat.data());
			glUniformMatrix4fv(glGetUniformLocation(program, "meshPose"), (GLsizei)skinnedPose.jointCount, 0, (*skinnedPoses).data());
		}

		IGL_INLINE void OculusVR::_setMaterialState(GLuint program, const ovrAvatarMaterialState* state, Eigen::Matrix4f* projectorInv) {
			// Assign the fragment uniforms
			glUniform1i(glGetUniformLocation(program, "useAlpha"), state->alphaMaskTextureID != 0);
			glUniform1i(glGetUniformLocation(program, "useNormalMap"), state->normalMapTextureID != 0);
			glUniform1i(glGetUniformLocation(program, "useRoughnessMap"), state->roughnessMapTextureID != 0);

			glUniform1f(glGetUniformLocation(program, "elapsedSeconds"), _elapsedSeconds);

			if (projectorInv)
			{
				glUniform1i(glGetUniformLocation(program, "useProjector"), 1);
				glUniformMatrix4fv(glGetUniformLocation(program, "projectorInv"), 1, 0, projectorInv->data());
			}
			else
			{
				glUniform1i(glGetUniformLocation(program, "useProjector"), 0);
			}

			int textureSlot = 1;
			glUniform4fv(glGetUniformLocation(program, "baseColor"), 1, &state->baseColor.x);
			glUniform1i(glGetUniformLocation(program, "baseMaskType"), state->baseMaskType);
			glUniform4fv(glGetUniformLocation(program, "baseMaskParameters"), 1, &state->baseMaskParameters.x);
			glUniform4fv(glGetUniformLocation(program, "baseMaskAxis"), 1, &state->baseMaskAxis.x);
			_setTextureSampler(program, textureSlot++, "alphaMask", state->alphaMaskTextureID);
			glUniform4fv(glGetUniformLocation(program, "alphaMaskScaleOffset"), 1, &state->alphaMaskScaleOffset.x);
			_setTextureSampler(program, textureSlot++, "clothingAlpha", _avatarCombinedMeshAlpha);
			glUniform4fv(glGetUniformLocation(program, "clothingAlphaScaleOffset"), 1, &_avatarCombinedMeshAlphaOffset.x);
			_setTextureSampler(program, textureSlot++, "normalMap", state->normalMapTextureID);
			glUniform4fv(glGetUniformLocation(program, "normalMapScaleOffset"), 1, &state->normalMapScaleOffset.x);
			_setTextureSampler(program, textureSlot++, "parallaxMap", state->parallaxMapTextureID);
			glUniform4fv(glGetUniformLocation(program, "parallaxMapScaleOffset"), 1, &state->parallaxMapScaleOffset.x);
			_setTextureSampler(program, textureSlot++, "roughnessMap", state->roughnessMapTextureID);
			glUniform4fv(glGetUniformLocation(program, "roughnessMapScaleOffset"), 1, &state->roughnessMapScaleOffset.x);

			struct LayerUniforms {
				int layerSamplerModes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				int layerBlendModes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				int layerMaskTypes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				ovrAvatarVector4f layerColors[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				int layerSurfaces[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				ovrAvatarAssetID layerSurfaceIDs[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				ovrAvatarVector4f layerSurfaceScaleOffsets[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				ovrAvatarVector4f layerSampleParameters[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				ovrAvatarVector4f layerMaskParameters[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
				ovrAvatarVector4f layerMaskAxes[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT];
			} layerUniforms;
			memset(&layerUniforms, 0, sizeof(layerUniforms));
			for (uint32_t i = 0; i < state->layerCount; ++i)
			{
				const ovrAvatarMaterialLayerState& layerState = state->layers[i];
				layerUniforms.layerSamplerModes[i] = layerState.sampleMode;
				layerUniforms.layerBlendModes[i] = layerState.blendMode;
				layerUniforms.layerMaskTypes[i] = layerState.maskType;
				layerUniforms.layerColors[i] = layerState.layerColor;
				layerUniforms.layerSurfaces[i] = textureSlot++;
				layerUniforms.layerSurfaceIDs[i] = layerState.sampleTexture;
				layerUniforms.layerSurfaceScaleOffsets[i] = layerState.sampleScaleOffset;
				layerUniforms.layerSampleParameters[i] = layerState.sampleParameters;
				layerUniforms.layerMaskParameters[i] = layerState.maskParameters;
				layerUniforms.layerMaskAxes[i] = layerState.maskAxis;
			}

			glUniform1i(glGetUniformLocation(program, "layerCount"), state->layerCount);
			glUniform1iv(glGetUniformLocation(program, "layerSamplerModes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerSamplerModes);
			glUniform1iv(glGetUniformLocation(program, "layerBlendModes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerBlendModes);
			glUniform1iv(glGetUniformLocation(program, "layerMaskTypes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerMaskTypes);
			glUniform4fv(glGetUniformLocation(program, "layerColors"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerColors);
			_setTextureSamplers(program, "layerSurfaces", OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, layerUniforms.layerSurfaces, layerUniforms.layerSurfaceIDs);
			glUniform4fv(glGetUniformLocation(program, "layerSurfaceScaleOffsets"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerSurfaceScaleOffsets);
			glUniform4fv(glGetUniformLocation(program, "layerSampleParameters"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerSampleParameters);
			glUniform4fv(glGetUniformLocation(program, "layerMaskParameters"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerMaskParameters);
			glUniform4fv(glGetUniformLocation(program, "layerMaskAxes"), OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT, (float*)layerUniforms.layerMaskAxes);

		}

		IGL_INLINE void OculusVR::_setTextureSampler(GLuint program, int textureUnit, const char uniformName[], ovrAvatarAssetID assetID) {
			GLuint textureID = 0;
			if (assetID)
			{
				void* data = _assetMap[assetID];
				AvatarTextureData* textureData = (AvatarTextureData*)data;
				textureID = textureData->textureID;
			}
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GL_TEXTURE_2D, textureID);
			glUniform1i(glGetUniformLocation(program, uniformName), textureUnit);
		}

		IGL_INLINE void OculusVR::_setTextureSamplers(GLuint program, const char uniformName[], size_t count, const int textureUnits[], const ovrAvatarAssetID assetIDs[]) {
			for (int i = 0; i < count; ++i)
			{
				ovrAvatarAssetID assetID = assetIDs[i];

				GLuint textureID = 0;
				if (assetID)
				{
					void* data = _assetMap[assetID];
					if (data)
					{
						AvatarTextureData* textureData = (AvatarTextureData*)data;
						textureID = textureData->textureID;
					}
				}
				glActiveTexture(GL_TEXTURE0 + textureUnits[i]);
				glBindTexture(GL_TEXTURE_2D, textureID);
			}
			GLint uniformLocation = glGetUniformLocation(program, uniformName);
			glUniform1iv(uniformLocation, (GLsizei)count, textureUnits);
		}

		IGL_INLINE void OculusVR::request_recenter() {
			ovr_RecenterTrackingOrigin(session);
			on_render_start();
			OVR::Vector3f eyePos = EyeRenderPose[1].Position;
			eye_pos_lock.lock();
			current_eye_origin = to_Eigen(eyePos);
			eye_pos_lock.unlock();
		}

		IGL_INLINE void OculusVR::navigate(ovrVector2f& thumb_pos, ViewerData& data) {
			if (data.V.rows() == 0) {
				return;
			}
			Eigen::Quaternionf old_rotation = Eigen::Quaternionf::Identity();
			data.set_mesh_model_translation();
			data.set_mesh_translation();
			igl::two_axis_valuator_fixed_up(2 * 1000, 2 * 1000, 0.2, old_rotation, 0, 0, thumb_pos.x * 1000, thumb_pos.y * 1000, data.mesh_trackball_angle); //Multiply width, heigth, x-pos and y-pos with 1000 because function takes ints
			data.rotate(data.mesh_trackball_angle);
			data.compute_normals();
			callback_button_down(THUMB_MOVE, Eigen::Vector3f());
			prev_sent = THUMB_MOVE;
		}

		Eigen::Vector3f OculusVR::to_Eigen(OVR::Vector3f& vec) {
			Eigen::Vector3f result;
			result << vec[0], vec[1], vec[2];
			return result;
		}

		Eigen::Matrix4f OculusVR::to_Eigen(OVR::Matrix4f& mat) {
			Eigen::Matrix4f result;
			for (int i = 0; i < 4; i++) {
				result.row(i) << mat.M[i][0], mat.M[i][1], mat.M[i][2], mat.M[i][3];
			}
			return result;
		}

		IGL_INLINE int OculusVR::eyeTextureWidth() {
			return eye_buffers[0]->eyeTextureSize.w;
		}

		IGL_INLINE int OculusVR::eyeTextureHeight() {
			return eye_buffers[0]->eyeTextureSize.h;
		}

		IGL_INLINE Eigen::Vector3f OculusVR::get_last_eye_origin() {
			std::lock_guard<std::mutex> guard1(mu_last_eye_origin);
			return current_eye_origin;
		}

		IGL_INLINE Eigen::Vector3f OculusVR::get_right_touch_direction() {
			std::lock_guard<std::mutex> guard1(mu_touch_dir);
			return right_touch_direction;
		}

		IGL_INLINE Eigen::Matrix4f OculusVR::get_start_action_view() {
			std::lock_guard<std::mutex> guard1(mu_start_view);
			return start_action_view;
		}

		IGL_INLINE void OculusVR::set_start_action_view(Eigen::Matrix4f new_start_action_view) {
			std::lock_guard<std::mutex> guard1(mu_start_view);
			start_action_view = new_start_action_view;
		}

	}
}