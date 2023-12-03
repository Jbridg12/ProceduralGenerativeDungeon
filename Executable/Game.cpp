//
// Game.cpp
//

#include "pch.h"
#include "Game.h"


//toreorganise
#include <fstream>

extern void ExitGame();

using namespace DirectX;
using namespace DirectX::SimpleMath;
using namespace ImGui;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false)
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
}

Game::~Game()
{
#ifdef DXTK_AUDIO
    if (m_audEngine)
    {
        m_audEngine->Suspend();
    }
#endif
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{

	m_input.Initialise(window);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

	//setup imgui.  its up here cos we need the window handle too
	//pulled from imgui directx11 example
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);		//tie to our window
	ImGui_ImplDX11_Init(m_deviceResources->GetD3DDevice(), m_deviceResources->GetD3DDeviceContext());	//tie to directx

	m_fullscreenRect.left = 0;
	m_fullscreenRect.top = 0;
	m_fullscreenRect.right = 800;
	m_fullscreenRect.bottom = 600;
   
    // Init MiniMap Location from Current Window Size
    RECT temp;
    temp = m_deviceResources->GetOutputSize();
    m_CameraViewRect.left = temp.right - 300;
    m_CameraViewRect.top = 0;
    m_CameraViewRect.right = temp.right;
    m_CameraViewRect.bottom = 240;

	//setup light
	m_Light.setAmbientColour(0.3f, 0.3f, 0.3f, 1.0f);
	m_Light.setDiffuseColour(1.0f, 1.0f, 1.0f, 1.0f);
	m_Light.setPosition(2.0f, 1.0f, 1.0f);
	m_Light.setDirection(-1.0f, -1.0f, 0.0f);

	//setup cameras
	m_Camera01.setPosition(Vector3(20.0f, 0.0f, 20.0f));
	m_Camera01.setRotation(Vector3(90.0f, 90.0f, 0.0f));

    m_MapCamera.setPosition(Vector3(20.0f, 25.0f, 20.0f));
    m_MapCamera.setRotation(Vector3(180.0f, 90.0f, 0.0f));

    //init gameplay feature (collectibles)
    m_collectiblesFound = 0;

	
#ifdef DXTK_AUDIO
    // Create DirectXTK for Audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audioEvent = 0;
    m_audioTimerAcc = 10.f;
    m_retryDefault = false;

    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"adpcmdroid.xwb");

    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_adpcm.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect2 = m_waveBank->CreateInstance(10);

    m_effect1->Play(true);
    m_effect2->Play();
#endif
}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
	//take in input
	m_input.Update();								//update the hardware
	m_gameInputCommands = m_input.getGameInput();	//retrieve the input for our game
	
	//Update all game objects
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

	//Render all game content. 
    Render();

#ifdef DXTK_AUDIO
    // Only update audio engine once per frame
    if (!m_audEngine->IsCriticalError() && m_audEngine->Update())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
#endif

	
}


// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{	
    double d_time = timer.GetElapsedSeconds();
	
    //this is hacky,  i dont like this here.  
	auto device = m_deviceResources->GetD3DDevice();

    Vector3 position = m_Camera01.getPosition(); // get position of player/camera

	if (m_gameInputCommands.left)
	{
		Vector3 rotation = m_Camera01.getRotation();
		rotation.y = rotation.y += m_Camera01.getRotationSpeed();
		m_Camera01.setRotation(rotation);
	}
	if (m_gameInputCommands.right)
	{
		Vector3 rotation = m_Camera01.getRotation();
		rotation.y = rotation.y -= m_Camera01.getRotationSpeed();
		m_Camera01.setRotation(rotation);
	}
	if (m_gameInputCommands.forward)
	{
		position += (m_Camera01.getForward()*m_Camera01.getMoveSpeed() * d_time); //add the forward vector
	}
	if (m_gameInputCommands.back)
	{
		position -= (m_Camera01.getForward()*m_Camera01.getMoveSpeed() * d_time); //add the forward vector
	}

    // Separate to not duplicate
    if (m_gameInputCommands.back || m_gameInputCommands.forward)
    {
        position = m_Terrain.CollideWithWall(position, m_Camera01.getPosition());
        position = m_Physics.CollideWithBall(position, m_Camera01.getPosition(), m_Camera01.getForward());
        m_Camera01.setPosition(position);

        if (m_Terrain.CollideWithCollectible(position))
            m_collectiblesFound++;

        position.y += 25.0f;
        m_MapCamera.setPosition(position);

    }

    if (m_gameInputCommands.generate)
    {
        m_Physics.ApplyForceOnObjectInRange(m_Camera01.getPosition(), m_Camera01.getForward());
    }

	m_Camera01.Update();	//camera updates
    m_MapCamera.Update();

	m_Terrain.Update();		//terrain update.  doesnt do anything at the moment. 
    m_Physics.Update(d_time);

	m_view = m_Camera01.getCameraMatrix();
    m_mapView = m_MapCamera.getCameraMatrix();
	m_world = Matrix::Identity;

	/*create our UI*/
	SetupGUI();

#ifdef DXTK_AUDIO
    m_audioTimerAcc -= (float)timer.GetElapsedSeconds();
    if (m_audioTimerAcc < 0)
    {
        if (m_retryDefault)
        {
            m_retryDefault = false;
            if (m_audEngine->Reset())
            {
                // Restart looping audio
                m_effect1->Play(true);
            }
        }
        else
        {
            m_audioTimerAcc = 4.f;

            m_waveBank->Play(m_audioEvent++);

            if (m_audioEvent >= 11)
                m_audioEvent = 0;
        }
    }
#endif

  
	if (m_input.Quit())
	{
		ExitGame();
	}
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{	
    // Don't try to render anything before the first Update.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    auto context = m_deviceResources->GetD3DDeviceContext();
	auto renderTargetView = m_deviceResources->GetRenderTargetView();
	auto depthTargetView = m_deviceResources->GetDepthStencilView();

    RECT windowDimensions = m_deviceResources->GetOutputSize();

	//Set Rendering states. 
	context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
	context->RSSetState(m_states->CullClockwise());
    //context->rssetstate(m_states->wireframe());

    RenderToTexture();

    PostProcessingSepia();

    // Render Collectible count
    char collectiblesString[32];
    sprintf(collectiblesString, "Collectibles Found: %d", m_collectiblesFound);

    // Manipulate text to Bottom Left
    // Draw Text to the screen
    m_sprites->Begin();
    m_font->DrawString(m_sprites.get(), collectiblesString, XMFLOAT2(windowDimensions.left, windowDimensions.bottom -50), Colors::Yellow);
    m_sprites->End();

    //render our GUI
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


    // Derive Minimap location from current window size
    RECT miniMap = windowDimensions;
    miniMap.left = miniMap.right - 300;
    miniMap.top = 0;
    miniMap.bottom = 240;

    // MiniMap
    m_sprites->Begin();
    m_sprites->Draw(m_FirstRenderPass->getShaderResourceView(), miniMap);
    m_sprites->End();

    // Show the new frame.
    m_deviceResources->Present();
}

// Render Setting to separate texture for mini-map effect
void Game::RenderToTexture()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTargetView = m_deviceResources->GetRenderTargetView();
    auto depthTargetView = m_deviceResources->GetDepthStencilView();


    // Set the render target to be the render to texture.
    m_FirstRenderPass->setRenderTarget(context);
    // Clear the render to texture.
    m_FirstRenderPass->clearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);

    //prepare transform for floor object. 
    m_world = SimpleMath::Matrix::Identity; //set world back to identity
    SimpleMath::Matrix newPosition3 = SimpleMath::Matrix::CreateTranslation(0.0f, -0.6f, 0.0f);
    SimpleMath::Matrix newScale = SimpleMath::Matrix::CreateScale(1.0);		//scale the terrain down a little. 
    m_world = m_world * newScale * newPosition3;

    //setup and draw cube
    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_mapView, &m_projection, &m_Light, m_texture1.Get());
    m_Terrain.Render(context);

    // Render Player icon
    m_world = SimpleMath::Matrix::Identity; //set world back to identity
    SimpleMath::Matrix playerPos = SimpleMath::Matrix::CreateTranslation(m_Camera01.getPosition());
    m_world = m_world * playerPos;

    //setup and draw cube
    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_mapView, &m_projection, &m_Light, m_texture3.Get());
    m_BasicModel.Render(context);


    // Render Collectibles
    for (int i = 0; i < COLLECTIBLE_COUNT; i++)
    {
        Vector3 currRender = m_Terrain.getCollectibles()[i];

        m_world = SimpleMath::Matrix::Identity; //set world back to identity
        SimpleMath::Matrix collPosition = SimpleMath::Matrix::CreateTranslation(currRender);
        m_world = m_world * collPosition;

        //setup and draw cube
        m_BasicShaderPair.EnableShader(context);
        m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_mapView, &m_projection, &m_Light, m_texture3.Get());
        m_BasicModel3.Render(context);
    }

    // Reset the render target back to the original back buffer and not the render to texture anymore.	
    context->OMSetRenderTargets(1, &renderTargetView, depthTargetView);
}

void Game::RenderSceneToTexture()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTargetView = m_deviceResources->GetRenderTargetView();
    auto depthTargetView = m_deviceResources->GetDepthStencilView();


    // Set the render target to be the render to texture.
    m_SceneRender->setRenderTarget(context);
    // Clear the render to texture.
    m_SceneRender->clearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);

    //prepare transform for floor object. 
    m_world = SimpleMath::Matrix::Identity; //set world back to identity
    SimpleMath::Matrix newPosition3 = SimpleMath::Matrix::CreateTranslation(0.0f, -0.6f, 0.0f);
    SimpleMath::Matrix newScale = SimpleMath::Matrix::CreateScale(1.0);		//scale the terrain down a little. 
    m_world = m_world * newScale * newPosition3;

    //setup and draw cube
    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture1.Get());
    m_Terrain.Render(context);


    // Render Collectibles
    for (int i = 0; i < COLLECTIBLE_COUNT; i++)
    {
        Vector3 currRender = m_Terrain.getCollectibles()[i];

        m_world = SimpleMath::Matrix::Identity; //set world back to identity
        SimpleMath::Matrix collPosition = SimpleMath::Matrix::CreateTranslation(currRender);
        SimpleMath::Matrix collVertAdjust = SimpleMath::Matrix::CreateTranslation(0.0f, 1.1f, 0.0f);
        SimpleMath::Matrix collRotation = SimpleMath::Matrix::CreateFromAxisAngle(SimpleMath::Vector3(-1.0f, 1.0f, 0.0f), (float)m_timer.GetTotalSeconds());
        m_world = m_world * collRotation * collPosition * collVertAdjust;

        //setup and draw cube
        m_BasicShaderPair.EnableShader(context);
        m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture3.Get());
        m_BasicModel3.Render(context);
    }

    //prepare transform for physics object. 
    m_world = SimpleMath::Matrix::Identity; //set world back to identity
    SimpleMath::Matrix physicsPosition = SimpleMath::Matrix::CreateTranslation(m_Physics.GetActivePosition());


    // Get perpendicular axis
    Vector3 rotationAxis;
    if (m_Physics.GetActiveVelocity().z == 0)
    {
        rotationAxis.x = 0;
    }
    else
    {
        rotationAxis.x = m_Physics.GetActiveVelocity().z / m_Physics.GetActiveVelocity().z;
    }

    if (m_Physics.GetActiveVelocity().x == 0)
    {
        rotationAxis.z = 0;
    }
    else
    {
        rotationAxis.z = -m_Physics.GetActiveVelocity().x / m_Physics.GetActiveVelocity().x;
    }
    rotationAxis.y = 0;

    // Using Quaternions for storing and using rotation tracking
    SimpleMath::Quaternion physicsRotation = m_Physics.GetActiveRotation();
    if (rotationAxis != Vector3().Zero)
    {
        SimpleMath::Quaternion temp = SimpleMath::Quaternion::CreateFromAxisAngle(rotationAxis, (float)m_timer.GetElapsedSeconds() * m_Physics.GetActiveVelocity().Length());
        temp.Normalize();
        physicsRotation *= temp;
        m_Physics.SetActiveRotation(physicsRotation);
    }

    m_world *= Matrix::CreateFromQuaternion(physicsRotation);
    m_world *= physicsPosition;
    
    //setup and draw ball
    m_BasicShaderPair.EnableShader(context);
    m_BasicShaderPair.SetShaderParameters(context, &m_world, &m_view, &m_projection, &m_Light, m_texture3.Get());
    m_BasicModel.Render(context);

}

// Perform post-processing ( Found on DirectX Wiki)
void Game::PostProcessingBloom()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTargetView = m_deviceResources->GetRenderTargetView();
    auto depthTargetView = m_deviceResources->GetDepthStencilView();
    
    RenderSceneToTexture();

    m_postProcess->SetEffect(BasicPostProcess::BloomExtract);
    m_postProcess->SetBloomExtractParameter(0.25f);
    
    m_blur1->setRenderTarget(context);
    m_blur1->clearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);

    m_postProcess->SetSourceTexture(m_SceneRender->getShaderResourceView());
    m_postProcess->Process(context);

    // Pass 2 (blur1 -> blur2)
    m_postProcess->SetEffect(BasicPostProcess::BloomBlur);
    m_postProcess->SetBloomBlurParameters(true, 4.f, 1.f);

    m_blur2->setRenderTarget(context);
    m_blur2->clearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);

    m_postProcess->SetSourceTexture(m_blur1->getShaderResourceView());
    m_postProcess->Process(context);

    // Pass 3 (blur2 -> blur1)
    m_postProcess->SetBloomBlurParameters(false, 4.f, 1.f);

    ID3D11ShaderResourceView* nullsrv[] = { nullptr, nullptr };
    context->PSSetShaderResources(0, 2, nullsrv);

    m_blur1->setRenderTarget(context);
    m_blur1->clearRenderTarget(context, 0.0f, 0.0f, 0.0f, 1.0f);

    m_postProcess->SetSourceTexture(m_blur2->getShaderResourceView());
    m_postProcess->Process(context);

    // Reset the render target back to the original back buffer and not the render to texture anymore.	
    // Pass 4 (scene+blur1 -> rt)
    m_betterPostProcess->SetEffect(DualPostProcess::BloomCombine);
    m_betterPostProcess->SetBloomCombineParameters(1.25f, 1.f, 1.f, 1.f);

    context->OMSetRenderTargets(1, &renderTargetView, depthTargetView);

    m_betterPostProcess->SetSourceTexture(m_SceneRender->getShaderResourceView());
    m_betterPostProcess->SetSourceTexture2(m_blur1->getShaderResourceView());
    m_betterPostProcess->Process(context);
}

// Created using help from above method from DirectX wiki
void Game::PostProcessingSepia()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTargetView = m_deviceResources->GetRenderTargetView();
    auto depthTargetView = m_deviceResources->GetDepthStencilView();

    RenderSceneToTexture();

    m_postProcess->SetEffect(BasicPostProcess::Sepia);

    context->OMSetRenderTargets(1, &renderTargetView, depthTargetView);

    m_postProcess->SetSourceTexture(m_SceneRender->getShaderResourceView());
    m_postProcess->Process(context);
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::Black);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}

#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
}

void Game::OnDeactivated()
{
}

void Game::OnSuspending()
{
#ifdef DXTK_AUDIO
    m_audEngine->Suspend();
#endif
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

#ifdef DXTK_AUDIO
    m_audEngine->Resume();
#endif
}

void Game::OnWindowMoved()
{
    auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}

#ifdef DXTK_AUDIO
void Game::NewAudioDevice()
{
    if (m_audEngine && !m_audEngine->IsAudioDevicePresent())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }
}
#endif

// Properties
void Game::GetDefaultSize(int& width, int& height) const
{
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_states = std::make_unique<CommonStates>(device);
    m_postProcess = std::make_unique<BasicPostProcess>(device);
    m_betterPostProcess = std::make_unique<DualPostProcess>(device);
    m_fxFactory = std::make_unique<EffectFactory>(device);
    m_sprites = std::make_unique<SpriteBatch>(context);
    m_font = std::make_unique<SpriteFont>(device, L"SegoeUI_18.spritefont");
	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

	//setup our terrain
	m_Terrain.Initialize(device, 128, 128);

    //setup physics engine
    m_Physics.Initialize(m_Terrain);

	//setup our test model
	m_BasicModel.InitializeSphere(device);
	m_BasicModel2.InitializeModel(device,"drone.obj");
	m_BasicModel3.InitializeBox(device, 1.0f, 1.0f, 1.0f);	//box includes dimensions

	//load and set up our Vertex and Pixel Shaders
	m_BasicShaderPair.InitStandard(device, L"light_vs.cso", L"light_ps.cso");

	//load Textures
	CreateDDSTextureFromFile(device, L"stone.dds",		    nullptr,	m_texture1.ReleaseAndGetAddressOf());
	CreateDDSTextureFromFile(device, L"EvilDrone_Diff.dds", nullptr,	m_texture2.ReleaseAndGetAddressOf());
	CreateDDSTextureFromFile(device, L"gold.dds",           nullptr,	m_texture3.ReleaseAndGetAddressOf());


	//Initialise Render to texture
	m_FirstRenderPass = new RenderTexture(device, 800, 600, 1, 2);	//for our rendering, We dont use the last two properties. but.  they cant be zero and they cant be the same. 
    m_SceneRender     = new RenderTexture(device, 800, 600, 1, 2);
    m_blur1           = new RenderTexture(device, 400, 300, 1, 2);
    m_blur2           = new RenderTexture(device, 400, 300, 1, 2);

}

// Allocate all memory resources that change on a window SizeChanged event.
void Game::CreateWindowSizeDependentResources()
{
    auto size = m_deviceResources->GetOutputSize();
    float aspectRatio = float(size.right) / float(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    // This is a simple example of change that can be made when the app is in
    // portrait or snapped view.
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }

    // This sample makes use of a right-handed coordinate system using row-major matrices.
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        100.0f
    );
}

void Game::SetupGUI()
{

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("PCG Dungeon Parameters");
        ImGui::InputFloat("PCGSeedChance", m_Terrain.GetPCGSeedChance());
        ImGui::InputInt("PCGIterations", m_Terrain.GetPCGIterations());
        ImGui::InputInt("PCGThreshold", m_Terrain.GetPCGThreshold());
        if (ImGui::Button("Generate", ImVec2(80, 60)))
        {
            m_Terrain.GenerateHeightMap(m_deviceResources->GetD3DDevice(), m_Camera01.getPosition());
        }

        ImGui::SliderFloat("Gravity", m_Physics.GravityGUI(), 0.0f, 1.0f);
        ImGui::SliderFloat("Friction", m_Physics.FrictionGUI(), 0.0f, 1.0f);
        ImGui::SliderFloat("Elasticty", m_Physics.ElasticityGUI(), 0.0f, 1.0f);
        ImGui::InputFloat("KickStrength", m_Physics.KickStrengthGUI());
        ImGui::InputFloat("Spawned Ball Mass", m_Physics.BallMassGUI());
        if (ImGui::Button("Spawn Ball", ImVec2(80, 60)))
        {
            m_Physics.SpawnBall(m_Camera01.getPosition() + m_Camera01.getForward() * Vector3(3, 0, 3), 30);
        }
	ImGui::End();
}


void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_font.reset();
	m_batch.reset();
	m_testmodel.reset();
    m_batchInputLayout.Reset();
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();
    CreateWindowSizeDependentResources();
}
#pragma endregion
