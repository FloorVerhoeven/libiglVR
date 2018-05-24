#ifndef IGL_OCULUSVR_H
#define IGL_OCULUSVR_H


#include <thread>
#include <atomic>
#include <mutex>
#include <igl/igl_inline.h>

#include "../gl.h"
#include <GLFW/glfw3.h>
#include "ViewerData.h"
#include <Eigen/Geometry>
#include "Extras/OVR_Math.h"
#include "igl/opengl/glfw/Viewer.h"

#include <OVR_Avatar.h>
#include <OVR_Platform.h>

namespace igl{
	namespace opengl{
		class OculusVR{
public:
	OculusVR() {};
			//OculusVR& operator = (const OculusVR& other) {

//			}

		//	OculusVR(const OculusVR& other) {
		//	}
			IGL_INLINE void init();
			IGL_INLINE bool init_VR_buffers(int window_width, int window_height);
			IGL_INLINE void on_render_start();
			IGL_INLINE void handle_input(std::atomic<bool>& update_screen_while_computing, ViewerData& current_data);
			IGL_INLINE void request_recenter();
			IGL_INLINE void handle_avatar_messages(igl::opengl::glfw::Viewer* viewer);
			IGL_INLINE void draw(std::vector<ViewerData>& data_list, GLFWwindow* window, ViewerCore& core, std::atomic<bool>& update_screen_while_computing);
			IGL_INLINE void submit_frame();
			IGL_INLINE void blit_mirror();
			IGL_INLINE void update_avatar();
			IGL_INLINE void ovrAvatarTransform_from_OVR(const ovrVector3f& position, const ovrQuatf& orientation, ovrAvatarTransform* target);
			IGL_INLINE void ovrAvatarHandInputState_from_OVR(const ovrAvatarTransform& transform, const ovrInputState& inputState, ovrHandType hand, ovrAvatarHandInputState* state);
			IGL_INLINE void handle_avatar_specification(const ovrAvatarMessage_AvatarSpecification* message);
			IGL_INLINE void handle_asset_loaded(const ovrAvatarMessage_AssetLoaded* message, igl::opengl::glfw::Viewer* viewer);
			IGL_INLINE void navigate(ovrVector2f& thumb_pos, ViewerData& current_data);
			IGL_INLINE static ViewerData* loadMesh(const ovrAvatarMeshAssetData* data, igl::opengl::glfw::Viewer* viewer);
			IGL_INLINE void render_avatar(ovrAvatar* avatar, uint32_t visibilityMask, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints);
			IGL_INLINE static void _computeWorldPose(const ovrAvatarSkinnedMeshPose& localPose, Eigen::Matrix4f* worldPose);
			IGL_INLINE static void EigenFromOvrAvatarTransform(const ovrAvatarTransform& transform, Eigen::Matrix4f& target);
			IGL_INLINE Eigen::Matrix4f* prepare_avatar_draw(const ovrAvatarRenderPart_SkinnedMeshRender* mesh, const ovrAvatarSkinnedMeshPose& skinnedPose, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos);
			IGL_INLINE void _setMeshState(GLuint program, const ovrAvatarTransform& localTransform, const ViewerData* data, const ovrAvatarSkinnedMeshPose& skinnedPose, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f proj, const Eigen::Vector3f& viewPos);
			IGL_INLINE void _renderSkinnedMeshPart(GLuint shader, const ovrAvatarRenderPart_SkinnedMeshRender* mesh, uint32_t visibilityMask, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints);
			IGL_INLINE int eyeTextureWidth();
			IGL_INLINE int eyeTextureHeight();

			IGL_INLINE Eigen::Vector3f to_Eigen(OVR::Vector3f& vec);
			IGL_INLINE Eigen::Matrix4f to_Eigen(OVR::Matrix4f& mat);

			IGL_INLINE Eigen::Vector3f get_last_eye_origin();
			IGL_INLINE Eigen::Vector3f get_right_touch_direction();
			IGL_INLINE Eigen::Matrix4f get_start_action_view();
			IGL_INLINE void set_start_action_view(Eigen::Matrix4f new_start_action_view);
	
			Eigen::Matrix4f start_action_view;
			Eigen::Vector3f right_touch_direction;
			Eigen::Vector3f current_eye_origin;

			std::mutex mu_start_view;
			std::mutex mu_touch_dir;
			std::mutex mu_last_eye_origin;
			std::unique_lock<std::mutex> eye_pos_lock;
			std::unique_lock<std::mutex> touch_dir_lock;

			enum ButtonCombo { A, B, THUMB, THUMB_MOVE, TRIG, GRIP, GRIPTRIG, NONE};

			std::function<void(ButtonCombo buttons, Eigen::Vector3f& hand_pos)> callback_button_down;


			//Variables for input handling
			ButtonCombo prev_press;
			ButtonCombo prev_sent;
			int count;
			ovrSessionStatus sessionStatus;
			ovrPosef handPoses[2];
			ovrInputState inputState;
			double displayMidpointSeconds;

private:

// A buffer struct used to store eye textures and framebuffers.
			// We create one instance for the left eye, one for the right eye.
			// Final rendering is done via blitting two separate frame buffers into one render target.
			struct OVR_buffer {
				IGL_INLINE OVR_buffer(const ovrSession &session, int eyeIdx);
				IGL_INLINE void OnRender();
				IGL_INLINE void OnRenderFinish();

				ovrSizei   eyeTextureSize;
				GLuint     eyeFbo = 0;
				GLuint     eyeTexId = 0;
				GLuint     depthBuffer = 0;

				ovrTextureSwapChain swapTextureChain = nullptr;
			};

			OVR_buffer *eye_buffers[2];
		};
	}
}
#ifndef IGL_STATIC_LIBRARY
#include "OculusVR.cpp"
#endif

#endif