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
#include "igl/opengl/glfw/imgui/ImGuiMenu.h"

#include <OVR_Avatar.h>
#include <OVR_Platform.h>

namespace igl{
	namespace opengl{
		class OculusVR{
public:
	OculusVR() {};

	typedef struct AvatarTextureData {
		GLuint textureID;
	} AvatarTextureData;

	typedef struct AvatarMeshData {
		GLuint vertexArray;
		GLuint vertexBuffer;
		GLuint elementBuffer;
		GLuint elementCount;
		Eigen::Matrix4f bindPose[OVR_AVATAR_MAXIMUM_JOINT_COUNT];
		Eigen::Matrix4f inverseBindPose[OVR_AVATAR_MAXIMUM_JOINT_COUNT];
	} AvatarMeshData;

			IGL_INLINE void init();
			IGL_INLINE bool init_VR_buffers(int window_width, int window_height);
			IGL_INLINE void on_render_start();
			IGL_INLINE void handle_input(std::atomic<bool>& update_screen_while_computing, ViewerData& current_data);
			IGL_INLINE void request_recenter();
			IGL_INLINE void handle_avatar_messages(igl::opengl::glfw::Viewer* viewer);
			IGL_INLINE void draw(std::vector<ViewerData>& data_list, glfw::ViewerPlugin* gui, GLFWwindow* window, ViewerCore& core, std::atomic<bool>& update_screen_while_computing);
			IGL_INLINE void submit_frame();
			IGL_INLINE void blit_mirror();
			IGL_INLINE void update_avatar(float deltaSeconds);
			IGL_INLINE void ovrAvatarTransform_from_OVR(const ovrVector3f& position, const ovrQuatf& orientation, ovrAvatarTransform* target);
			IGL_INLINE void ovrAvatarHandInputState_from_OVR(const ovrAvatarTransform& transform, const ovrInputState& inputState, ovrHandType hand, ovrAvatarHandInputState* state);
			IGL_INLINE void handle_avatar_specification(const ovrAvatarMessage_AvatarSpecification* message);
			IGL_INLINE void handle_asset_loaded(const ovrAvatarMessage_AssetLoaded* message, igl::opengl::glfw::Viewer* viewer);
			IGL_INLINE void navigate(ovrVector2f& thumb_pos, ViewerData& current_data);
			IGL_INLINE static AvatarMeshData* loadMesh(const ovrAvatarMeshAssetData* data);
			IGL_INLINE static AvatarTextureData* loadTexture(const ovrAvatarTextureAssetData* data);
			IGL_INLINE void render_avatar(ovrAvatar* avatar, uint32_t visibilityMask, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints);
			IGL_INLINE static void _computeWorldPose(const ovrAvatarSkinnedMeshPose& localPose, Eigen::Matrix4f* worldPose);
			IGL_INLINE static void EigenFromOvrAvatarTransform(const ovrAvatarTransform& transform, Eigen::Matrix4f& target);
			IGL_INLINE void _setMeshState(GLuint program, const ovrAvatarTransform& localTransform, const AvatarMeshData* data, const ovrAvatarSkinnedMeshPose& skinnedPose, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f proj, const Eigen::Vector3f& viewPos);
			IGL_INLINE void _renderSkinnedMeshPart(const ovrAvatarRenderPart_SkinnedMeshRender* mesh, uint32_t visibilityMask, const Eigen::Matrix4f& world, const Eigen::Matrix4f& view, const Eigen::Matrix4f& proj, const Eigen::Vector3f& viewPos, bool renderJoints);
			IGL_INLINE void _setMaterialState(GLuint program, const ovrAvatarMaterialState* state, Eigen::Matrix4f* projectorInv);
			IGL_INLINE void _setTextureSampler(GLuint program, int textureUnit, const char uniformName[], ovrAvatarAssetID assetID);
			IGL_INLINE void _setTextureSamplers(GLuint program, const char uniformName[], size_t count, const int textureUnits[], const ovrAvatarAssetID assetIDs[]);
			IGL_INLINE int eyeTextureWidth();
			IGL_INLINE int eyeTextureHeight();
			IGL_INLINE GLuint get_skinned_avatar_shader();

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
				IGL_INLINE OVR_buffer(const ovrSession &session);
				IGL_INLINE void OnRender();
				IGL_INLINE void OnRenderFinish();
				IGL_INLINE void OnRenderHud();
				IGL_INLINE void OnRenderFinishHud();

				ovrSizei   eyeTextureSize;
				GLuint     eyeFbo = 0;
				GLuint     eyeTexId = 0;
				GLuint     depthBuffer = 0;

				ovrTextureSwapChain swapTextureChain = nullptr;
			};

			OVR_buffer *eye_buffers[2];
			OVR_buffer *hud_buffer;
			std::chrono::steady_clock::time_point lastTime;

			
		};
	}
}
#ifndef IGL_STATIC_LIBRARY
#include "OculusVR.cpp"
#endif

#endif