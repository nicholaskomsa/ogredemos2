#pragma once

#include <OgreHlmsBufferManager.h>
#include <OgreConstBufferPool.h>
#include <OgreMatrix4.h>
#include <OgreHlmsPbs.h>
#include <OgreHeaderPrefix.h>
#include <OgreHlmsListener.h>
#include "OgreShaderParams.h"


#include "OgreHlmsManager.h"
#include "OgreHlmsListener.h"
#include "OgreLwString.h"


#include "OgreViewport.h"
#include "OgreHighLevelGpuProgramManager.h"
#include "OgreHighLevelGpuProgram.h"
#include "OgreForward3D.h"
#include "Cubemaps/OgreParallaxCorrectedCubemap.h"
#include "OgreIrradianceVolume.h"
#include "OgreCamera.h"

#include "OgreSceneManager.h"
#include "Compositor/OgreCompositorShadowNode.h"
#include "Vao/OgreVaoManager.h"
#include "Vao/OgreConstBufferPacked.h"

#include "CommandBuffer/OgreCommandBuffer.h"
#include "CommandBuffer/OgreCbTexture.h"
#include "CommandBuffer/OgreCbShaderBuffer.h"

#include <ogrerenderqueue.h>
#include "OgreTextureGpuManager.h"
#include "OgreStagingTexture.h"

namespace Ogre {
	class SceneManager;
	class CompositorShadowNode;

}

class HlmsWindListener : public Ogre::HlmsListener {


	float mWindStrength{ 0.5 };
	float mGlobalTime{ 0 };

public:
	HlmsWindListener() = default;
	virtual ~HlmsWindListener() = default;

	void setTime(float time) {
		mGlobalTime = time;
	}
	void addTime(float time) {
		mGlobalTime += time;
	}

	virtual Ogre::uint32 getPassBufferSize(const Ogre::CompositorShadowNode* shadowNode, bool casterPass,
		bool dualParaboloid, Ogre::SceneManager* sceneManager) const {

		Ogre::uint32 size = 0;

		if (!casterPass) {

			unsigned int fogSize = sizeof(float) * 4 * 2; // float4 + float4 in bytes
			unsigned int windSize = sizeof(float);
			unsigned int timeSize = sizeof(float);

			size += fogSize + windSize + timeSize;
		}
		return size;
	}

	virtual float* preparePassBuffer(const Ogre::CompositorShadowNode* shadowNode, bool casterPass,
		bool dualParaboloid, Ogre::SceneManager* sceneManager,
		float* passBufferPtr) {

		if (!casterPass)
		{
			//Linear Fog parameters
			*passBufferPtr++ = sceneManager->getFogStart();
			*passBufferPtr++ = sceneManager->getFogEnd();
			*passBufferPtr++ = 0.0;
			*passBufferPtr++ = 0.0f;

			*passBufferPtr++ = sceneManager->getFogColour().r;
			*passBufferPtr++ = sceneManager->getFogColour().g;
			*passBufferPtr++ = sceneManager->getFogColour().b;
			*passBufferPtr++ = 1.0f;

			*passBufferPtr++ = mWindStrength;

			*passBufferPtr++ = mGlobalTime;
		}
		return passBufferPtr;
	}
};

class HlmsWind : public Ogre::HlmsPbs {

	HlmsWindListener mWindListener;

	Ogre::HlmsSamplerblock const* mNoiseSamplerBlock{ nullptr }, const* mWindFactorSamplerBlock{ nullptr };

	Ogre::TextureGpu* mNoiseTexture{ nullptr };
	Ogre::TextureGpu* mWindFactorTexture{ nullptr };

	void calculateHashForPreCreate(Ogre::Renderable* renderable, Ogre::PiecesMap* inOutPieces) override {

		HlmsPbs::calculateHashForPreCreate(renderable, inOutPieces);

		setProperty("wind_enabled", 1);
	}

	void loadTexturesAndSamplers(Ogre::SceneManager* manager) {

		Ogre::TextureGpuManager* textureManager =
			manager->getDestinationRenderSystem()->getTextureGpuManager();

		Ogre::HlmsSamplerblock samplerblock;
		samplerblock.mU = Ogre::TextureAddressingMode::TAM_WRAP;
		samplerblock.mV = Ogre::TextureAddressingMode::TAM_WRAP;
		samplerblock.mW = Ogre::TextureAddressingMode::TAM_WRAP;
		samplerblock.mMaxAnisotropy = 8;
		samplerblock.mMagFilter = Ogre::FO_ANISOTROPIC;
		mNoiseSamplerBlock = Ogre::Root::getSingleton().getHlmsManager()->getSamplerblock(samplerblock);


		samplerblock.mU = Ogre::TextureAddressingMode::TAM_CLAMP;
		samplerblock.mV = Ogre::TextureAddressingMode::TAM_CLAMP;
		samplerblock.mW = Ogre::TextureAddressingMode::TAM_CLAMP;
		samplerblock.mMaxAnisotropy = 0;
		samplerblock.mMagFilter = Ogre::FO_NONE;

		mWindFactorSamplerBlock = Ogre::Root::getSingleton().getHlmsManager()->getSamplerblock(samplerblock);


		mNoiseTexture = textureManager->createOrRetrieveTexture(
			"windNoise.dds",
			Ogre::GpuPageOutStrategy::Discard,
			Ogre::TextureFlags::PrefersLoadingFromFileAsSRGB,
			Ogre::TextureTypes::Type3D,
			Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);

		mNoiseTexture->scheduleTransitionTo(Ogre::GpuResidency::Resident);

		mWindFactorTexture = textureManager->createOrRetrieveTexture(
			"windFactor.png",
			Ogre::GpuPageOutStrategy::Discard,
			Ogre::TextureFlags::PrefersLoadingFromFileAsSRGB,
			Ogre::TextureTypes::Type2D,
			Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);

		mWindFactorTexture->scheduleTransitionTo(Ogre::GpuResidency::Resident);
	}

public:
	HlmsWind(Ogre::Archive* dataFolder, Ogre::ArchiveVec* libraryFolders)
		: Ogre::HlmsPbs(dataFolder, libraryFolders)
	{
		mType = Ogre::HLMS_USER0;
		mTypeName = "Wind";
		mTypeNameStr = "Wind";

		setListener(&mWindListener);

		mReservedTexSlots = 2u;
	}

	virtual ~HlmsWind() = default;

	void addTime(float time) {
		mWindListener.addTime(time);
	}

	void setup(Ogre::SceneManager* manager) {
		loadTexturesAndSamplers(manager);
	}

	void shutdown(Ogre::SceneManager* manager) {
		Ogre::TextureGpuManager* textureManager = manager->getDestinationRenderSystem()->getTextureGpuManager();
		textureManager->destroyTexture(mNoiseTexture);
		mNoiseTexture = nullptr;
		textureManager->destroyTexture(mWindFactorTexture);
		mWindFactorTexture = nullptr;

		Ogre::Root::getSingleton().getHlmsManager()->destroySamplerblock(mNoiseSamplerBlock);
		mNoiseSamplerBlock = nullptr;
		Ogre::Root::getSingleton().getHlmsManager()->destroySamplerblock(mWindFactorSamplerBlock);
		mWindFactorSamplerBlock = nullptr;

	}

	void notifyPropertiesMergedPreGenerationStep(void) {

		HlmsPbs::notifyPropertiesMergedPreGenerationStep();

		setTextureReg(Ogre::VertexShader, "texPerlinNoise", 14);

		setTextureReg(Ogre::VertexShader, "texWindFactor", 15);
	}

	static void getDefaultPaths(Ogre::String& outDataFolderPath, Ogre::StringVector& outLibraryFoldersPaths) {

		//We need to know what RenderSystem is currently in use, as the
		//name of the compatible shading language is part of the path
		Ogre::RenderSystem* renderSystem = Ogre::Root::getSingleton().getRenderSystem();
		Ogre::String shaderSyntax = "GLSL";
		if (renderSystem->getName() == "Direct3D11 Rendering Subsystem")
			shaderSyntax = "HLSL";
		else if (renderSystem->getName() == "Metal Rendering Subsystem")
			shaderSyntax = "Metal";


		//Fill the library folder paths with the relevant folders
		outLibraryFoldersPaths.clear();
		outLibraryFoldersPaths.push_back("Hlms/Common/" + shaderSyntax);
		outLibraryFoldersPaths.push_back("Hlms/Common/Any");
		outLibraryFoldersPaths.push_back("Hlms/Pbs/Any");
		outLibraryFoldersPaths.push_back("Hlms/Pbs/Any/Main");
		outLibraryFoldersPaths.push_back("Hlms/Wind/Any");

		//Fill the data folder path
		outDataFolderPath = "Hlms/pbs/" + shaderSyntax;
	}

	Ogre::uint32 fillBuffersForV1(const Ogre::HlmsCache* cache,
		const Ogre::QueuedRenderable& queuedRenderable,
		bool casterPass, Ogre::uint32 lastCacheHash,
		Ogre::CommandBuffer* commandBuffer)
	{
		return fillBuffersFor(cache, queuedRenderable, casterPass,
			lastCacheHash, commandBuffer, true);
	}
	//-----------------------------------------------------------------------------------
	Ogre::uint32 fillBuffersForV2(const Ogre::HlmsCache* cache,
		const Ogre::QueuedRenderable& queuedRenderable,
		bool casterPass, Ogre::uint32 lastCacheHash,
		Ogre::CommandBuffer* commandBuffer)
	{
		return fillBuffersFor(cache, queuedRenderable, casterPass,
			lastCacheHash, commandBuffer, false);
	}
	//-----------------------------------------------------------------------------------
	Ogre::uint32 fillBuffersFor(const Ogre::HlmsCache* cache, const Ogre::QueuedRenderable& queuedRenderable,
		bool casterPass, Ogre::uint32 lastCacheHash,
		Ogre::CommandBuffer* commandBuffer, bool isV1)
	{

		*commandBuffer->addCommand<Ogre::CbTexture>() = Ogre::CbTexture(14, const_cast<Ogre::TextureGpu*>(mNoiseTexture), mNoiseSamplerBlock);

		*commandBuffer->addCommand<Ogre::CbTexture>() = Ogre::CbTexture(15, const_cast<Ogre::TextureGpu*>(mWindFactorTexture), mWindFactorSamplerBlock);

		return Ogre::HlmsPbs::fillBuffersFor(cache, queuedRenderable, casterPass, lastCacheHash, commandBuffer, isV1);
	}

};