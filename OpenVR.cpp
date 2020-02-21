#include "OpenVR.hpp"
#include <Scene/Scene.hpp>
#include <Scene/GUI.hpp>
#include <Content/Font.hpp>
#include <Scene/MeshRenderer.hpp>
#include <assimp/pbrmaterial.h>

using namespace std;

ENGINE_PLUGIN(OpenVR)

OpenVR::OpenVR() : mScene(nullptr), mCamera(nullptr), mInput(nullptr){
	mEnabled = true;
	mVRDevice = new OpenVRDevice();
	
}
OpenVR::~OpenVR() {
	mScene->RemoveObject(mCamera);
	mScene->RemoveObject(mCameraBase);
	for (Object* obj : mObjects)
		mScene->RemoveObject(obj);
	delete mMirrorTex;
}

void OpenVR::PreInstanceInit(Instance* instance)
{
	std::vector< std::string > requiredInstanceExtensions;
	mVRDevice->GetVulkanInstanceExtensionsRequired(requiredInstanceExtensions);
	for (std::string ex : requiredInstanceExtensions)
	{
		instance->RequestInstanceExtension(ex);
	}

	uint64_t device;
	vr::VRSystem()->GetOutputDevice(&device, vr::TextureType_Vulkan, (VkInstance_T*)instance);

	//instance->RequestInstanceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	instance->RequestInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

}

void OpenVR::PreDeviceInit(Instance* instance, VkPhysicalDevice device)
{
	std::vector< std::string > requiredDeviceExtensions;
	mVRDevice->GetVulkanDeviceExtensionsRequired(device, requiredDeviceExtensions);
	for (std::string ex : requiredDeviceExtensions)
	{
		instance->RequestDeviceExtension(ex);
	}

	instance->RequestDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

bool OpenVR::Init(Scene* scene) {

	mScene = scene;
	mInput = mScene->InputManager()->GetFirst<MouseKeyboardInput>();

#pragma region load glTF
	string folder = "Assets/Models/";
	string file = "cornellbox.gltf";

	shared_ptr<Material> opaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	opaque->EnableKeyword("TEXTURED");
	opaque->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> alphaClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	alphaClip->RenderQueue(5000);
	alphaClip->BlendMode(BLEND_MODE_ALPHA);
	alphaClip->CullMode(VK_CULL_MODE_NONE);
	alphaClip->EnableKeyword("TEXTURED");
	alphaClip->EnableKeyword("ALPHA_CLIP");
	alphaClip->EnableKeyword("TWO_SIDED");
	alphaClip->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> alphaBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
	alphaBlend->RenderQueue(5000);
	alphaBlend->BlendMode(BLEND_MODE_ALPHA);
	alphaBlend->CullMode(VK_CULL_MODE_NONE);
	alphaBlend->EnableKeyword("TEXTURED");
	alphaBlend->EnableKeyword("TWO_SIDED");
	alphaBlend->SetParameter("TextureST", float4(1, 1, 0, 0));

	shared_ptr<Material> curOpaque = nullptr;
	shared_ptr<Material> curClip = nullptr;
	shared_ptr<Material> curBlend = nullptr;

	uint32_t arraySize =
		mScene->AssetManager()->LoadShader("Shaders/pbr.stm")->GetGraphics(PASS_MAIN, { "TEXTURED" })->mDescriptorBindings.at("MainTextures").second.descriptorCount;

	uint32_t opaque_i = 0;
	uint32_t clip_i = 0;
	uint32_t blend_i = 0;

	auto matfunc = [&](Scene* scene, aiMaterial* aimaterial) {
		aiString alphaMode;
		if (aimaterial->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS) {
			if (alphaMode == aiString("MASK")) return alphaClip;
			if (alphaMode == aiString("BLEND")) return alphaBlend;
		}
		return opaque;
	};
	auto objfunc = [&](Scene* scene, Object* object, aiMaterial* aimaterial) {
		MeshRenderer* renderer = dynamic_cast<MeshRenderer*>(object);
		if (!renderer) return;

		Material* mat = renderer->Material();
		uint32_t i;

		if (mat == opaque.get()) {
			i = opaque_i;
			opaque_i++;
			if (opaque_i >= arraySize) curOpaque.reset();
			if (!curOpaque) {
				opaque_i = opaque_i % arraySize;
				curOpaque = make_shared<Material>("PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curOpaque->EnableKeyword("TEXTURED");
				curOpaque->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curOpaque);
			mat = curOpaque.get();

		}
		else if (mat == alphaClip.get()) {
			i = clip_i;
			clip_i++;
			if (clip_i >= arraySize) curClip.reset();
			if (!curClip) {
				clip_i = clip_i % arraySize;
				curClip = make_shared<Material>("Cutout PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curClip->RenderQueue(5000);
				curClip->BlendMode(BLEND_MODE_ALPHA);
				curClip->CullMode(VK_CULL_MODE_NONE);
				curClip->EnableKeyword("TEXTURED");
				curClip->EnableKeyword("ALPHA_CLIP");
				curClip->EnableKeyword("TWO_SIDED");
				curClip->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curClip);
			mat = curClip.get();

		}
		else if (mat == alphaBlend.get()) {
			i = blend_i;
			blend_i++;
			if (blend_i >= 64) curBlend.reset();
			if (!curBlend) {
				blend_i = blend_i % arraySize;
				curBlend = make_shared<Material>("Transparent PBR", mScene->AssetManager()->LoadShader("Shaders/pbr.stm"));
				curBlend->RenderQueue(5000);
				curBlend->BlendMode(BLEND_MODE_ALPHA);
				curBlend->CullMode(VK_CULL_MODE_NONE);
				curBlend->EnableKeyword("TEXTURED");
				curBlend->EnableKeyword("TWO_SIDED");
				curBlend->SetParameter("TextureST", float4(1, 1, 0, 0));
			}
			renderer->Material(curBlend);
			mat = curBlend.get();

		}
		else return;

		aiColor3D emissiveColor(0);
		aiColor4D baseColor(1);
		float metallic = 1.f;
		float roughness = 1.f;
		aiString baseColorTexture, metalRoughTexture, normalTexture, emissiveTexture;

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_TEXTURE, &baseColorTexture) == AI_SUCCESS && baseColorTexture.length) {
			mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture(folder + baseColorTexture.C_Str()));
			baseColor = aiColor4D(1);
		}
		else
			mat->SetParameter("MainTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/white.png"));

		if (aimaterial->GetTexture(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE, &metalRoughTexture) == AI_SUCCESS && metalRoughTexture.length)
			mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture(folder + metalRoughTexture.C_Str(), false));
		else
			mat->SetParameter("MaskTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/mask.png", false));

		if (aimaterial->GetTexture(aiTextureType_NORMALS, 0, &normalTexture) == AI_SUCCESS && normalTexture.length)
			mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture(folder + normalTexture.C_Str(), false));
		else
			mat->SetParameter("NormalTextures", i, scene->AssetManager()->LoadTexture("Assets/Textures/bump.png", false));

		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, baseColor);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic);
		aimaterial->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness);
		aimaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor);

		renderer->PushConstant("TextureIndex", i);
		renderer->PushConstant("Color", float4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
		renderer->PushConstant("Roughness", roughness);
		renderer->PushConstant("Metallic", metallic);
		renderer->PushConstant("Emission", float3(emissiveColor.r, emissiveColor.g, emissiveColor.b));
	};

	Object* root = mScene->LoadModelScene(folder + file, matfunc, objfunc, .6f, 1.f, .05f, .0015f);

	root->LocalRotation(quaternion(float3(0, PI / 2, 0)));
	queue<Object*> nodes;
	nodes.push(root);
	while (nodes.size()) {
		Object* o = nodes.front();
		nodes.pop();
		for (uint32_t i = 0; i < o->ChildCount(); i++)
			nodes.push(o->Child(i));

		mObjects.push_back(o);
		if (Light* l = dynamic_cast<Light*>(o)) {
			if (l->Type() == LIGHT_TYPE_SUN) {
				l->CascadeCount(1);
				l->ShadowDistance(30);
			}
		}
	}
#pragma endregion

	mScene->Environment()->EnableCelestials(false);
	mScene->Environment()->EnableScattering(false);
	mScene->Environment()->AmbientLight(.6f);

	
#pragma region Camera setup
	shared_ptr<Object> cameraBase = make_shared<Object>("CameraBase");
	mScene->AddObject(cameraBase);
	mCameraBase = cameraBase.get();
	mCameraBase->LocalPosition(0, .5f, 0);


	uint32_t renderWidth = 0;
	uint32_t renderHeight = 0;
	mVRDevice->System()->GetRecommendedRenderTargetSize(&renderWidth, &renderHeight);
	fprintf_color(COLOR_GREEN, stderr, "Created stereo camera of size %dx%d\n", renderWidth, renderHeight);


	//vector<VkFormat> colorFormats{ VK_FORMAT_R8G8B8A8_UNORM };
	//Framebuffer* f = new Framebuffer("Openvr Camera", scene->Instance()->Device(), renderWidth, renderHeight, colorFormats, VK_FORMAT_D32_SFLOAT, VK_SAMPLE_COUNT_1_BIT, {}, VK_ATTACHMENT_LOAD_OP_CLEAR);
	shared_ptr<Camera> camera = make_shared<Camera>("Camera", scene->Instance()->Window());
	mScene->AddObject(camera);
	camera->Near(.01f);
	camera->Far(1024.f);
	camera->FieldOfView(radians(65.f));
	camera->LocalPosition(0, 0, 0);
	//camera->
	mCamera = camera.get();
	mCameraBase->AddChild(mCamera);

	camera->StereoMode(STEREO_SBS_HORIZONTAL);


	mVRDevice->CalculateEyeAdjustment();
	float4x4 eye = (inverse(mVRDevice->LeftEyeMatrix()));
	camera->HeadToEye(eye, EYE_LEFT);
	eye = (inverse(mVRDevice->RightEyeMatrix()));
	camera->HeadToEye(eye, EYE_RIGHT);

	mVRDevice->CalculateProjectionMatrices();
	float4x4 proj = (mVRDevice->LeftProjection());
	float4x4 flipy = float4x4(1);
	//flipy[1][1] = -1;
	//proj[1][1] = -proj[1][1];
	camera->Projection(proj * flipy, EYE_LEFT);
	proj = (mVRDevice->RightProjection());
	//proj[1][1] = -proj[1][1];
	camera->Projection(proj * flipy, EYE_RIGHT);
#pragma endregion

	VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	mMirrorTex = new Texture("Mirror Texture", 
		scene->Instance()->Device(), 
		mCamera->FramebufferWidth(), mCamera->FramebufferHeight(), 1, 
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, 
		flags);

	return true;
}

void OpenVR::Update() {
	mVRDevice->CalculateEyeAdjustment();
	//mCamera->EyeTransform(mVRDevice->LeftEyeMatrix(), EYE_LEFT);
	//mCamera->EyeTransform(mVRDevice->RightEyeMatrix(), EYE_RIGHT);

	mVRDevice->CalculateProjectionMatrices();
	//mCamera->Projection(mVRDevice->LeftProjection(), EYE_LEFT);
	//mCamera->Projection(mVRDevice->RightProjection(), EYE_RIGHT);


	if (mInput->KeyDownFirst(KEY_F1))
		mScene->DrawGizmos(!mScene->DrawGizmos());
	//if (mInput->KeyDownFirst(KEY_TILDE))
	//	mShowPerformance = !mShowPerformance;

	mVRDevice->Update();
}

void OpenVR::PreRender(CommandBuffer* commandBuffer, Camera* camera, PassType pass)
{

	camera->LocalPosition(mVRDevice->Position());
	camera->LocalRotation(mVRDevice->Rotation());
}

/*
void OpenVR::DrawGizmos(CommandBuffer* commandBuffer, Camera* camera) {
}

void OpenVR::PostRenderScene(CommandBuffer* commandBuffer, Camera* camera, PassType pass) {
}
*/

void OpenVR::PostProcess(CommandBuffer* commandBuffer, Camera* camera) {
	if (camera != mCamera)
	{
		return;
	}

	//mCamera->Resolve(commandBuffer);
	VkPipelineStageFlags srcStage, dstStage, srcStage2, dstStage2;
	//mCamera->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);
	VkImageMemoryBarrier barrier[2] = {};
	barrier[0] = mCamera->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStage, dstStage);
	barrier[1] = mMirrorTex->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, srcStage2, dstStage2);
	
	srcStage = srcStage | srcStage2;
	dstStage = dstStage | dstStage2;
	vkCmdPipelineBarrier(*commandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr,
		0, nullptr,
		2, barrier);

	VkImageSubresourceLayers srcLayers = {};
	srcLayers.baseArrayLayer = 0;
	srcLayers.layerCount = 1;
	srcLayers.mipLevel = 0;
	srcLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VkImageSubresourceLayers dstLayers = {};
	dstLayers.baseArrayLayer = 0;
	dstLayers.layerCount = 1;
	dstLayers.mipLevel = 0;
	dstLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VkExtent3D extent = {};
	extent.width = mCamera->ResolveBuffer()->Width();
	extent.height = mCamera->ResolveBuffer()->Height();
	extent.depth = mCamera->ResolveBuffer()->Depth();

	VkImageCopy copy = {};
	copy.srcSubresource = srcLayers;
	copy.dstSubresource = dstLayers;
	copy.extent = extent;
	//copy.
	vkCmdCopyImage(*commandBuffer, mCamera->ResolveBuffer()->Image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mMirrorTex->Image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	VkImageMemoryBarrier barrier2[2] = {};
	barrier2[0] = mCamera->ResolveBuffer()->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, srcStage, dstStage);
	barrier2[1] = mMirrorTex->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStage2, dstStage2);

	srcStage = srcStage | srcStage2;
	dstStage = dstStage | dstStage2;
	vkCmdPipelineBarrier(*commandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr,
		0, nullptr,
		2, barrier2);
		
}

void OpenVR::PreSwap()
{
	Texture* tex = mMirrorTex;

	// Submit to SteamVR
	vr::VRTextureBounds_t leftBounds;
	leftBounds.uMin = 0.0f;
	leftBounds.uMax = 0.5f;
	leftBounds.vMin = 0.0f;
	leftBounds.vMax = 1.0f;

	vr::VRTextureBounds_t rightBounds;
	rightBounds.uMin = 0.5f;
	rightBounds.uMax = 1.0f;
	rightBounds.vMin = 0.0f;
	rightBounds.vMax = 1.0f;

	//tex->TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer);

	vr::VRVulkanTextureData_t vulkanData;
	vulkanData.m_nImage = (uint64_t)(tex->Image());
	vulkanData.m_pDevice = (VkDevice_T*)mScene->Instance()->Device();
	vulkanData.m_pPhysicalDevice = (VkPhysicalDevice_T*)mScene->Instance()->Device()->PhysicalDevice();
	vulkanData.m_pInstance = (VkInstance_T*)mScene->Instance()->Device()->Instance();
	vulkanData.m_pQueue = (VkQueue_T*)mScene->Instance()->Device()->GraphicsQueue();
	vulkanData.m_nQueueFamilyIndex = mScene->Instance()->Device()->GraphicsQueueFamily();

	vulkanData.m_nHeight = tex->Height();
	vulkanData.m_nWidth = tex->Width();
	vulkanData.m_nFormat = tex->Format();
	vulkanData.m_nSampleCount = tex->SampleCount();

	vr::Texture_t texture = { &vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
	vr::EVRCompositorError error;
	error = vr::VRCompositor()->Submit(vr::Eye_Left, &texture, nullptr);
	if (error != vr::VRCompositorError_None)
	{
		if (error == vr::VRCompositorError_TextureUsesUnsupportedFormat)
		{
			printf_color(COLOR_RED, "Texture format unsupported: %s\n", FormatToString(tex->Format()));

			if (tex->Usage() & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
				printf_color(COLOR_RED, "TRANSFER_SRC_BIT, ");
			if (tex->Usage() & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
				printf_color(COLOR_RED, "TRANSFER_DST_BIT, ");
			if (tex->Usage() & VK_IMAGE_USAGE_SAMPLED_BIT)
				printf_color(COLOR_RED, "SAMPLED_BIT, ");
			if (tex->Usage() & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
				printf_color(COLOR_RED, "COLOR_ATTACHMENT_BIT, ");
			printf("\n");
			printf_color(COLOR_RED, "Texture name: %s, mips: %d\nsamples: %d\n", tex->mName, tex->MipLevels(), tex->SampleCount());
		}
		else
		{
			printf_color(COLOR_RED, "Compositor error on left eye submission: %d\n", error);
		}
	}
	vr::VRCompositor()->Submit(vr::Eye_Right, &texture, &rightBounds);
	if (error != vr::VRCompositorError_None)
	{
		//printf_color(COLOR_RED, "Compositor error on right eye submission: %d\n", error);
	}
}