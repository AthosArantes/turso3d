#include "UiManager.h"
#include "RmlUi/RmlFile.h"
#include "RmlUi/RmlRenderer.h"
#include "RmlUi/RmlSystem.h"
#include <Turso3D/Graphics/Graphics.h>
#include <Turso3D/Graphics/ShaderProgram.h>
#include <Turso3D/Graphics/FrameBuffer.h>
#include <Turso3D/Graphics/Texture.h>
#include <Turso3D/Math/IntVector2.h>
#include <Turso3D/IO/Log.h>
#include <RmlUi/Core.h>
#include <utility>

using namespace Turso3D;

namespace
{
	constexpr const char* DefaultFonts[] = {
		"ui/fonts/FiraGO-Bold.ttf",
		"ui/fonts/FiraGO-BoldItalic.ttf",
		"ui/fonts/FiraGO-Book.ttf",
		"ui/fonts/FiraGO-BookItalic.ttf",
		"ui/fonts/FiraGO-ExtraBold.ttf",
		"ui/fonts/FiraGO-ExtraBoldItalic.ttf",
		"ui/fonts/FiraGO-Heavy.ttf",
		"ui/fonts/FiraGO-HeavyItalic.ttf",
		"ui/fonts/FiraGO-Italic.ttf",
		"ui/fonts/FiraGO-Light.ttf",
		"ui/fonts/FiraGO-LightItalic.ttf",
		"ui/fonts/FiraGO-Medium.ttf",
		"ui/fonts/FiraGO-MediumItalic.ttf",
		"ui/fonts/FiraGO-Regular.ttf",
		"ui/fonts/FiraGO-SemiBold.ttf",
		"ui/fonts/FiraGO-SemiBoldItalic.ttf"
	};

	struct FrameStats
	{
		double previousFrameTime;
		int fps;
	};
}

struct UiManager::Middleware
{
	std::unique_ptr<RmlFile> rmlFile;
	std::unique_ptr<RmlSystem> rmlSystem;
	std::unique_ptr<RmlRenderer> rmlRenderer;

	// Frame stats

	Rml::Context* frameStatsContext;
	Rml::DataModelHandle frameStatsModel;
	FrameStats frameStats;
};

// ==========================================================================================
UiManager::UiManager()
{
	impl = std::make_unique<Middleware>();
}

UiManager::~UiManager()
{
	if (Rml::RemoveContext(impl->frameStatsContext->GetName())) {
		impl->frameStatsContext = nullptr;
	}

	Rml::ReleaseFontResources();
	Rml::ReleaseTextures();

	// Shutdown RmlUi
	Rml::Shutdown();
}

void UiManager::Initialize()
{
	Log::Scope logScope {"UiManager::Initialize"};

	composeProgram = Graphics::CreateProgram("PostProcess/GuiCompose.glsl", "", "");

	impl->rmlFile = std::make_unique<RmlFile>();
	impl->rmlSystem = std::make_unique<RmlSystem>();
	impl->rmlRenderer = std::make_unique<RmlRenderer>();

	Rml::SetFileInterface(impl->rmlFile.get());
	Rml::SetSystemInterface(impl->rmlSystem.get());
	Rml::SetRenderInterface(impl->rmlRenderer.get());
	Rml::Initialise();

	// Load default fonts
	for (int i = 0; i < std::size(DefaultFonts); ++i) {
		if (!Rml::LoadFontFace(DefaultFonts[i])) {
			LOG_ERROR("Failed to load font: {:s}", DefaultFonts[i]);
		}
	}

	// Create stats context and its data model
	{
		const IntVector2& rs = Graphics::RenderSize();
		Rml::Context* ctx = Rml::CreateContext("frame.stats", Rml::Vector2i {rs.x, rs.y});

		Rml::DataModelConstructor dm = ctx->CreateDataModel("stats");
		dm.Bind("fps", &impl->frameStats.fps);
		dm.Bind("frame_time", &impl->frameStats.previousFrameTime);

		// Load a document
		Rml::ElementDocument* doc = ctx->LoadDocument("ui/stats.rml");
		doc->Show();

		impl->frameStatsContext = ctx;
		impl->frameStatsModel = dm.GetModelHandle();
	}
}

void UiManager::UpdateBuffers(const IntVector2& size)
{
	impl->rmlRenderer->UpdateBuffers(size, 1);

	impl->frameStatsContext->SetDimensions(Rml::Vector2i(size.x, size.y));
	impl->frameStatsContext->Update();
}

void UiManager::Update(double dt)
{
	TURSO3D_GRAPHICS_MARKER("Ui");

	impl->frameStats.previousFrameTime = dt;
	impl->frameStats.fps = static_cast<int>(1.0 / dt);
	impl->frameStatsModel.DirtyAllVariables();
	impl->frameStatsContext->Update();

	// Render
	impl->rmlRenderer->BeginRender();
	impl->frameStatsContext->Render();
	impl->rmlRenderer->EndRender();
}

void UiManager::Compose(Texture* background, Texture* blurredBackground)
{
	TURSO3D_GRAPHICS_MARKER("UI Compose");

	Graphics::BindProgram(composeProgram.get());

	Graphics::BindTexture(0, background);
	Graphics::BindTexture(1, blurredBackground);
	Graphics::BindTexture(2, impl->rmlRenderer->GetTexture());
	Graphics::BindTexture(3, impl->rmlRenderer->GetMaskTexture());

	Graphics::DrawQuad();
}
