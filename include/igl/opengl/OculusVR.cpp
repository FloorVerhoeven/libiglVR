#define IGL_OCULUSVR_CPP
#include "OculusVR.h"
#include <igl/opengl/glfw/Viewer.h>

#ifdef _WIN32
#  include <windows.h>
#  undef max
#  undef min
#endif

#define FAIL(X) throw std::runtime_error(X)
#define MIRROR_SAMPLE_APP_ID "2094457250596307"

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

		static ovrAvatar* _avatar;
		static bool _combineMeshes = true;
		static bool _waitingOnCombinedMesh = false;
		static size_t _loadingAssets;
		static ovrAvatarAssetID _avatarCombinedMeshAlpha = 0;
		static ovrAvatarVector4f _avatarCombinedMeshAlphaOffset;
		static std::map<ovrAvatarAssetID, void*> _assetMap;
		static float _elapsedSeconds;

		struct TextureData {
			GLuint textureID;
		};


		IGL_INLINE void OculusVR::init() {
			eye_pos_lock = std::unique_lock<std::mutex>(mu_last_eye_origin, std::defer_lock);
			touch_dir_lock = std::unique_lock<std::mutex>(mu_touch_dir, std::defer_lock);
			callback_button_down = nullptr;
			// Initialize the OVR Platform module
			if (!OVR_SUCCESS(ovr_PlatformInitializeWindows(MIRROR_SAMPLE_APP_ID))) {
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


			if (!init_VR_buffers(windowSize.w, windowSize.h)) {
				FAIL("Something went wrong when initializing VR buffers");
			}

			// Initialize the avatar module
			ovrAvatar_Initialize(MIRROR_SAMPLE_APP_ID);
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

		IGL_INLINE void OculusVR::handle_avatar_messages(igl::opengl::glfw::Viewer* viewer) {
			lastTime = std::chrono::steady_clock::now();
			while (ovrAvatarMessage* message = ovrAvatarMessage_Pop()){
				switch (ovrAvatarMessage_GetType(message)){
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

				
					for (int i = 0; i < data_list.size(); i++) { //TODO: this currently also goes over the avatar parts but doesn't draw them (it does bind them though...)
						if (_avatar && !_loadingAssets && !_waitingOnCombinedMesh && data_list[i].avatar_V.rows() > 0) {
							//prepare_avatar_draw(mesh->skinnedPose, world, view, proj, to_Eigen(shiftedEyePos));
							//core.draw_avatar_part(data_list[i], view, proj, to_Eigen(shiftedEyePos));
						}
						else {
							core.draw(data_list[i], true, true, view, proj);
						}
					}
						if (_avatar && !_loadingAssets && !_waitingOnCombinedMesh) {
						render_avatar(_avatar, ovrAvatarVisibilityFlag_FirstPerson, view, proj, to_Eigen(shiftedEyePos), false);
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

		IGL_INLINE void OculusVR::handle_avatar_specification(const ovrAvatarMessage_AvatarSpecification* message){
			// Create the avatar instance
			_avatar = ovrAvatar_Create(message->avatarSpec, ovrAvatarCapability_Hands);
			ovrAvatar_SetLeftControllerVisibility(_avatar, 0);
			ovrAvatar_SetRightControllerVisibility(_avatar, 0);
			// Trigger load operations for all of the assets referenced by the avatar
			uint32_t refCount = ovrAvatar_GetReferencedAssetCount(_avatar);
			for (uint32_t i = 0; i < refCount; ++i){
				ovrAvatarAssetID id = ovrAvatar_GetReferencedAsset(_avatar, i);
				ovrAvatarAsset_BeginLoading(id);
				++_loadingAssets;
			}
			printf("Loading %d assets...\r\n", _loadingAssets);
		}

		IGL_INLINE void OculusVR::handle_asset_loaded(const ovrAvatarMessage_AssetLoaded* message, igl::opengl::glfw::Viewer* viewer){
			// Determine the type of the asset that got loaded
			ovrAvatarAssetType assetType = ovrAvatarAsset_GetType(message->asset);
			void* data = nullptr;

			// Call the appropriate loader function
			switch (assetType){
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

			if (assetType == ovrAvatarAssetType_CombinedMesh){
				std::cout << "unexpectedly encountered a CombinedMesh type. These do not work yet. " << std::endl;
			//	uint32_t idCount = 0;
				//ovrAvatarAsset_GetCombinedMeshIDs(message->asset, &idCount);
				//_loadingAssets -= idCount;
				//ovrAvatar_GetCombinedMeshAlphaData(_avatar, &_avatarCombinedMeshAlpha, &_avatarCombinedMeshAlphaOffset);
			}
			else{
				--_loadingAssets;
			}

			printf("Loading %d assets...\r\n", _loadingAssets);
		}

		IGL_INLINE ViewerData* OculusVR::loadMesh(const ovrAvatarMeshAssetData* data){
			//MeshData* mesh = new MeshData();
			//(*viewer).append_mesh();
			std::cout << "nr f: " << data->indexCount << std::endl;
			std::cout << "nr V: " << data->vertexCount << std::endl;
			ViewerData* mesh = new ViewerData();
			Eigen::MatrixXd V(data->vertexCount, 3);
			Eigen::MatrixXi F(data->indexCount/3, 3);
			Eigen::MatrixXd normals(data->vertexCount, 3);
			Eigen::MatrixXd tangents(data->vertexCount, 4);
			Eigen::MatrixXd tex(data->vertexCount, 2);
			Eigen::MatrixXi poseIndices(data->vertexCount, 4);
			Eigen::MatrixXd poseWeights(data->vertexCount, 4);

			for (int i = 0; i < data->vertexCount; i++) {
				V.row(i) << data->vertexBuffer[i].x, data->vertexBuffer[i].y, data->vertexBuffer[i].z;
				normals.row(i) << data->vertexBuffer[i].nx, data->vertexBuffer[i].ny, data->vertexBuffer[i].nz;
				tangents.row(i) << data->vertexBuffer[i].tx, data->vertexBuffer[i].ty, data->vertexBuffer[i].tz, data->vertexBuffer[i].tw;
				tex.row(i) << data->vertexBuffer[i].u, data->vertexBuffer[i].v;
				poseIndices.row(i) << (int)data->vertexBuffer[i].blendIndices[0], (int)data->vertexBuffer[i].blendIndices[1], (int)data->vertexBuffer[i].blendIndices[2], (int)data->vertexBuffer[i].blendIndices[3];
				poseWeights.row(i) << data->vertexBuffer[i].blendWeights[0], data->vertexBuffer[i].blendWeights[1], data->vertexBuffer[i].blendWeights[2], data->vertexBuffer[i].blendWeights[3];
			}

			for (int i = 0; i < data->indexCount / 3; i++) {
				F.row(i) << (int)data->indexBuffer[i * 3 + 0], (int)data->indexBuffer[i * 3 + 1], (int)data->indexBuffer[i * 3 + 2];
			}
			mesh->set_avatar(V, F, normals, tangents, tex, poseIndices, poseWeights);
			

			// Translate the bind pose from local to world
			Eigen::Matrix4f* bindPoses = (Eigen::Matrix4f*)alloca(sizeof(Eigen::Matrix4f) *data->skinnedBindPose.jointCount);
			_computeWorldPose(data->skinnedBindPose, bindPoses);
			mesh->set_inverse_bind_pose(bindPoses, data->skinnedBindPose.jointCount);
			return mesh;
		}

		IGL_INLINE void OculusVR::_computeWorldPose(const ovrAvatarSkinnedMeshPose& localPose, Eigen::Matrix4f* worldPose)
		 {
			 for (uint32_t i = 0; i < localPose.jointCount; ++i)
			 {
				 Eigen::Matrix4f local;
				 EigenFromOvrAvatarTransform(localPose.jointTransform[i], local);

				 int parentIndex = localPose.jointParents[i];
				 if (parentIndex < 0)
				 {
					 worldPose[i] = local;
				 }
				 else
				 {
					 worldPose[i] = worldPose[parentIndex] * local;
				 }
			 }
		 }

		IGL_INLINE void OculusVR::EigenFromOvrAvatarTransform(const ovrAvatarTransform& transform, Eigen::Matrix4f& target) {
			  Eigen::Matrix4f scale = Eigen::Matrix4f::Identity();
			  scale.topLeftCorner(3,3) = Scaling(transform.scale.x, transform.scale.y, transform.scale.z);
			  Eigen::Matrix4f translation = Eigen::Matrix4f::Zero();
			  translation.col(3) << transform.position.x, transform.position.y, transform.position.z, 1.0f;
			  Eigen::Matrix4f rot = Eigen::Matrix4f::Identity();
			  rot.topLeftCorner(3, 3) = Eigen::Quaternionf(transform.orientation.w, transform.orientation.x, transform.orientation.y, transform.orientation.z).matrix();
			  target = translation * rot * scale;
		 }

		IGL_INLINE OculusVR::TextureData* OculusVR::loadTexture(const ovrAvatarTextureAssetData* data) {
			// Create a texture
			TextureData* texture = new TextureData();
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
		
		IGL_INLINE void OculusVR::render_avatar(ovrAvatar* avatar, uint32_t visibilityMask, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints){
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
						_renderSkinnedMeshPart(ovrAvatarRenderPart_GetSkinnedMeshRender(renderPart), visibilityMask, world, view, proj, viewPos, renderJoints);
						break;
					case ovrAvatarRenderPartType_SkinnedMeshRenderPBS:
				//		_renderSkinnedMeshPartPBS(ovrAvatarRenderPart_GetSkinnedMeshRenderPBS(renderPart), visibilityMask, world, view, proj, viewPos, renderJoints);
						break;
					case ovrAvatarRenderPartType_ProjectorRender:
					//	_renderProjector(ovrAvatarRenderPart_GetProjectorRender(renderPart), avatar, visibilityMask, world, view, proj, viewPos);
						break;
					}
				}
			}
		}

		IGL_INLINE Eigen::Matrix4f* OculusVR::prepare_avatar_draw(const ovrAvatarRenderPart_SkinnedMeshRender* mesh, const ovrAvatarSkinnedMeshPose& skinnedPose, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos) {
			Eigen::Matrix4f local;
			EigenFromOvrAvatarTransform(mesh->localTransform, local);
			Eigen::Matrix4f worldMat = world * local;
			Eigen::Matrix4f viewProjMat = proj * view;
			ViewerData* data = (ViewerData*) _assetMap[mesh->meshAssetID];
			// Compute the skinned pose
			Eigen::Matrix4f* skinnedPoses = (Eigen::Matrix4f*)alloca(sizeof(Eigen::Matrix4f) * skinnedPose.jointCount);
			_computeWorldPose(skinnedPose, skinnedPoses);
			for (uint32_t i = 0; i < skinnedPose.jointCount; ++i)
			{
				skinnedPoses[i] = skinnedPoses[i] * data->inverse_bind_pose[i];
			}

			return skinnedPoses;
		}

		IGL_INLINE void OculusVR::_renderSkinnedMeshPart(const ovrAvatarRenderPart_SkinnedMeshRender* mesh, uint32_t visibilityMask, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints)
		{
			// If this part isn't visible from the viewpoint we're rendering from, do nothing
			if ((mesh->visibilityMask & visibilityMask) == 0)
			{
				return;
			}

			// Get the GL mesh data for this mesh's asset
			ViewerData* data = (ViewerData*)_assetMap[mesh->meshAssetID];
			data->updateGL(*data, false, data->meshgl);
			glUseProgram(data->meshgl.shader_avatar);
			//std::cout << "test" << data->meshgl.avatar_V_vbo << std::endl;
			data->meshgl.bind_avatar();
			// Apply the vertex state
			_setMeshState(data->meshgl.shader_avatar, mesh->localTransform, data, mesh->skinnedPose, world, view, proj, viewPos);

			// Apply the material state
			_setMaterialState(data->meshgl.shader_avatar, &mesh->materialState, nullptr);

			// Draw the mesh
		//	glBindVertexArray(data->vertexArray);
			glDepthFunc(GL_LESS);

			// Write to depth first for self-occlusion
			if (mesh->visibilityMask & ovrAvatarVisibilityFlag_SelfOccluding)
			{
			//	glDepthMask(GL_TRUE);
			//	glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		//		std::cout << data->meshgl.avatar_F_vbo << std::endl;
		//		std::cout << data->avatar_F.rows() << std::endl;
				glDrawElements(GL_TRIANGLES, 3 * data->meshgl.avatar_F_vbo.rows(), GL_UNSIGNED_SHORT, 0);
			//	glDepthFunc(GL_EQUAL);
			}

			// Render to color buffer
		//	glDepthMask(GL_FALSE);
		//	glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glDrawElements(GL_TRIANGLES, 3 * data->meshgl.avatar_F_vbo.rows(), GL_UNSIGNED_SHORT, 0);
			glBindVertexArray(0);

			/*if (renderJoints)
			{
				Eigen::Matrix4f local;
				EigenFromOvrAvatarTransform(mesh->localTransform, local);
				glDepthFunc(GL_ALWAYS);
				_renderPose(proj * view * world * local, mesh->skinnedPose);
			}*/
		}

		IGL_INLINE void OculusVR::_setMeshState(GLuint program, const ovrAvatarTransform& localTransform, const ViewerData* data, const ovrAvatarSkinnedMeshPose& skinnedPose, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f proj, const Eigen::Vector3f& viewPos){
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
				skinnedPoses[i] = skinnedPoses[i] * data->inverse_bind_pose[i];
			}


			// Pass the world view position to the shader for view-dependent rendering
			glUniform3fv(glGetUniformLocation(program, "viewPos"), 1, viewPos.data());

			// Assign the vertex uniforms
			glUniformMatrix4fv(glGetUniformLocation(program, "world"), 1, 0, worldMat.data());
			glUniformMatrix4fv(glGetUniformLocation(program, "viewProj"), 1, 0, viewProjMat.data());
			glUniformMatrix4fv(glGetUniformLocation(program, "meshPose"), (GLsizei)skinnedPose.jointCount, 0, skinnedPoses->data()); //TODO: not sure if last param is correct
		}

		IGL_INLINE void OculusVR::_setMaterialState(GLuint program, const ovrAvatarMaterialState* state, Eigen::Matrix4f* projectorInv){
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

		IGL_INLINE void OculusVR::_setTextureSampler(GLuint program, int textureUnit, const char uniformName[], ovrAvatarAssetID assetID){
			GLuint textureID = 0;
			if (assetID)
			{
				void* data = _assetMap[assetID];
				TextureData* textureData = (TextureData*)data;
				textureID = textureData->textureID;
			}
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GL_TEXTURE_2D, textureID);
			glUniform1i(glGetUniformLocation(program, uniformName), textureUnit);
		}

		IGL_INLINE void OculusVR::_setTextureSamplers(GLuint program, const char uniformName[], size_t count, const int textureUnits[], const ovrAvatarAssetID assetIDs[]){
			for (int i = 0; i < count; ++i)
			{
				ovrAvatarAssetID assetID = assetIDs[i];

				GLuint textureID = 0;
				if (assetID)
				{
					void* data = _assetMap[assetID];
					if (data)
					{
						TextureData* textureData = (TextureData*)data;
						textureID = textureData->textureID;
					}
				}
				glActiveTexture(GL_TEXTURE0 + textureUnits[i]);
				glBindTexture(GL_TEXTURE_2D, textureID);
			}
			GLint uniformLocation = glGetUniformLocation(program, uniformName);
			glUniform1iv(uniformLocation, (GLsizei)count, textureUnits);
		}

		/*IGL_INLINE void OculusVR::_renderPose(const Eigen::Matrix4f& worldViewProj, const ovrAvatarSkinnedMeshPose& pose)
		{
			Eigen::Matrix4f* skinnedPoses = (Eigen::Matrix4f*)alloca(sizeof(Eigen::Matrix4f) * pose.jointCount);
			_computeWorldPose(pose, skinnedPoses);
			for (uint32_t i = 1; i < pose.jointCount; ++i)
			{
				int parent = pose.jointParents[i];
				_renderDebugLine(worldViewProj, glm::vec3(skinnedPoses[parent][3]), glm::vec3(skinnedPoses[i][3]), Eigen::Vector4f(1, 1, 1, 1), Eigen::Vector4f(1, 0, 0, 1));
			}
		}*/

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