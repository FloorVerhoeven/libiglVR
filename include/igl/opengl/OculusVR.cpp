#define IGL_OCULUSVR_CPP
#include "OculusVR.h"

#ifdef _WIN32
#  include <windows.h>
#  undef max
#  undef min
#endif

#define FAIL(X) throw std::runtime_error(X)

#include <igl/igl_inline.h>
#include <igl/two_axis_valuator_fixed_up.h>
#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_CAPI_Util.h>
#include <Extras/OVR_StereoProjection.h>
#include "Extras/OVR_Math.h"

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


		IGL_INLINE void OculusVR::init() {
			eye_pos_lock = std::unique_lock<std::mutex>(mu_last_eye_origin, std::defer_lock);
			touch_dir_lock = std::unique_lock<std::mutex>(mu_touch_dir, std::defer_lock);
			callback_button_down = nullptr;


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


			if (!init_VR_buffers(windowSize.w, windowSize.h)) {
				FAIL("Something went wrong when initializing VR buffers");
			}

			ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);
			ovr_RecenterTrackingOrigin(session);

			on_render_start();
			OVR::Vector3f eyePos = EyeRenderPose[1].Position;
			eye_pos_lock.lock();
			current_eye_origin = to_Eigen(eyePos);
			eye_pos_lock.unlock();

			prev_press = NONE;
			prev_sent = NONE;
			count = 0;
		}

		IGL_INLINE bool OculusVR::init_VR_buffers(int window_width, int window_height) {
			for (int eye = 0; eye < 2; eye++) {
				eye_buffers[eye] = new OVR_buffer(session, eye);
				ovrEyeRenderDesc& erd = eyeRenderDesc[eye] = ovr_GetRenderDesc(session, (ovrEyeType)eye, hmdDesc.DefaultEyeFov[eye]);
			}

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
				else if (inputState.Buttons & ovrButton_RThumb) {
					count = (prev_press == THUMB) ? count + 1 : 1;
					prev_press = THUMB;
				}
				else if (inputState.HandTrigger[ovrHand_Right] > 0.5f && inputState.IndexTrigger[ovrHand_Right] > 0.5f) {
					count = (prev_press == GRIPTRIG) ? count + 1 : 1;
					prev_press = GRIPTRIG;
				}
				else if (inputState.HandTrigger[ovrHand_Right] > 0.5f && inputState.IndexTrigger[ovrHand_Right] <= 0.2f) {
					count = (prev_press == GRIP) ? count + 1 : 1;
					prev_press = GRIP;

				}
				else if (inputState.HandTrigger[ovrHand_Right] <= 0.2f && inputState.IndexTrigger[ovrHand_Right] > 0.5f) {
					count = (prev_press == TRIG) ? count + 1 : 1;
					prev_press = TRIG;
				}
				else if (inputState.Thumbstick[ovrHand_Right].x > 0.1f || inputState.Thumbstick[ovrHand_Right].x < -0.1f || inputState.Thumbstick[ovrHand_Right].y > 0.1f || inputState.Thumbstick[ovrHand_Right].y < -0.1f) {
					navigate(inputState.Thumbstick[ovrHand_Right], data);
				}
				else if (inputState.HandTrigger[ovrHand_Right] <= 0.2f && inputState.IndexTrigger[ovrHand_Right] <= 0.2f && !(inputState.Buttons & ovrButton_A) && !(inputState.Buttons & ovrButton_B) && !(inputState.Buttons & ovrButton_X) && !(inputState.Buttons & ovrButton_Y)) {
					count = (prev_press == NONE) ? count + 1 : 1;
					prev_press = NONE;
				}

				if (count >= 3) { //Only do a callback when we have at least 3 of the same buttonCombos in a row (to prevent doing a GRIP/TRIG action when we were actually starting up a GRIPTRIG)
					touch_dir_lock.lock();
					right_touch_direction = to_Eigen(OVR::Matrix4f(handPoses[ovrHand_Right].Orientation).Transform(OVR::Vector3f(0, 0, -1)));
					touch_dir_lock.unlock();

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

		IGL_INLINE void OculusVR::draw(std::vector<ViewerData>& data_list, GLFWwindow* window, ViewerCore& core, std::atomic<bool>& update_screen_while_computing) {
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

				on_render_start();
				for (int eye = 0; eye < 2; eye++)
				{
					eye_buffers[eye]->OnRender();

					OVR::Matrix4f finalRollPitchYaw = OVR::Matrix4f(EyeRenderPose[eye].Orientation);
					OVR::Vector3f finalUp = finalRollPitchYaw.Transform(OVR::Vector3f(0, 1, 0));
					OVR::Vector3f finalForward = finalRollPitchYaw.Transform(OVR::Vector3f(0, 0, -1));
					OVR::Vector3f shiftedEyePos = OVR::Vector3f(EyeRenderPose[eye].Position);

					Eigen::Matrix4f view = to_Eigen(OVR::Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp)); //OVRLookATRH gives exact same result as igl::look_at with same input
					OVR::Matrix4f proj_tmp = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], 0.01f, 1000.0f, (ovrProjection_ClipRangeOpenGL));
					Eigen::Matrix4f proj = to_Eigen(proj_tmp);

					for (int i = 0; i < data_list.size(); i++) {
						core.draw(data_list[i], true, true, view, proj);
					}

					eye_buffers[eye]->OnRenderFinish();
					ovr_CommitTextureSwapChain(session, eye_buffers[eye]->swapTextureChain);

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

		IGL_INLINE void OculusVR::OVR_buffer::OnRender() {
			int currentIndex;
			ovr_GetTextureSwapChainCurrentIndex(session, swapTextureChain, &currentIndex);
			ovr_GetTextureSwapChainBufferGL(session, swapTextureChain, currentIndex, &eyeTexId);

			// Switch to eye render target
			glBindFramebuffer(GL_FRAMEBUFFER, eyeFbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeTexId, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthBuffer, 0);

			glViewport(0, 0, eyeTextureSize.w, eyeTextureSize.h);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_FRAMEBUFFER_SRGB);
		}

		IGL_INLINE void OculusVR::OVR_buffer::OnRenderFinish() {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		IGL_INLINE void OculusVR::submit_frame() {
			// create the main eye layer
			ovrLayerEyeFov eyeLayer;
			eyeLayer.Header.Type = ovrLayerType_EyeFov;
			eyeLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.

			for (int eye = 0; eye < 2; eye++)
			{
				eyeLayer.ColorTexture[eye] = eye_buffers[eye]->swapTextureChain;
				eyeLayer.Viewport[eye] = OVR::Recti(eye_buffers[eye]->eyeTextureSize);
				eyeLayer.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
				eyeLayer.RenderPose[eye] = EyeRenderPose[eye];
				eyeLayer.SensorSampleTime = sensorSampleTime;
			}

			// append all the layers to global list
			ovrLayerHeader* layerList = &eyeLayer.Header;
			ovrViewScaleDesc viewScaleDesc;
			viewScaleDesc.HmdSpaceToWorldScaleInMeters = 10.0f;
			ovrResult result = ovr_SubmitFrame(session, frame, &viewScaleDesc, &layerList, 1);
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

		IGL_INLINE void OculusVR::request_recenter() {
			ovr_RecenterTrackingOrigin(session);
			on_render_start();
			OVR::Vector3f eyePos = EyeRenderPose[1].Position;
			eye_pos_lock.lock();
			current_eye_origin = to_Eigen(eyePos);
			eye_pos_lock.unlock();
		}

		IGL_INLINE void OculusVR::navigate(ovrVector2f& thumb_pos, ViewerData& data) {
			data.set_mesh_model_translation();
			std::cout << data.mesh_model_translation << std::endl << std::endl;
			Eigen::Quaternionf old_rotation = data.mesh_trackball_angle;
			igl::two_axis_valuator_fixed_up(2 * 1000, 2 * 1000, 0.2, old_rotation, 0, 0, thumb_pos.x * 1000, thumb_pos.y * 1000, data.mesh_trackball_angle); //Multiply width, heigth, x-pos and y-pos with 1000 because function takes ints
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