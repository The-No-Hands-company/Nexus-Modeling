# Changelog

## [v0.23] — 2026-06-02

### Graphics Kernel

#### Instant-NGP Hash-Grid NeRF — Track 1 (Month 82)
- `InstantNGPSettings` struct — `enabled`, `hashGridLevels` (16), `hashGridFeatures` (2),
  `hashTableSize` (19).
- `Renderer::setInstantNGPSettings` / `instantNGPSettings()` — stored on renderer.
- `RendererSettings::enableInstantNGP` — global gate flag (default `false`).
- `FrameStats::instantNGPActive` — `true` when hash-grid NeRF dispatch ran.
- `FrameStats::instantNGPHashLevels` — hash grid levels evaluated this frame.
- `Renderer::render()`: after NeRF pass, when enabled, dispatches one 8×8-tile compute
  pass per hash grid level; accelerates NeRF inference via multi-resolution spatial features.
- New test file `tests/kernel/test_InstantNGP.cpp` — 8 Null-backend tests.

#### Dynamic NeRF (D-NeRF) — Track 2 (Month 83)
- `DynamicNeRFSettings` struct — `enabled`, `warpNetworkLayers` (6), `timeEmbeddingDim` (256),
  `deformationScale` (1.0).
- `Renderer::setDynamicNeRFSettings` / `dynamicNeRFSettings()` — stored on renderer.
- `RendererSettings::enableDynamicNeRF` — global gate flag (default `false`).
- `FrameStats::dynamicNeRFActive` — `true` when D-NeRF warp dispatch ran.
- `FrameStats::dynamicNeRFWarpPasses` — warp network passes evaluated this frame
  (`warpNetworkLayers + 1`).
- `Renderer::render()`: after NeRF pass, when enabled, dispatches warp field passes + a
  deformed radiance march for time-conditioned dynamic scene rendering.
- New test file `tests/kernel/test_DynamicNeRF.cpp` — 8 Null-backend tests.

#### Holographic Wavefront Encoding — Track 3 (Month 84)
- `HolographicWavefrontSettings` struct — `enabled`, `depthSlices` (8),
  `pupilDiameterMm` (4.0), `wavelengthNm` (532.0).
- `Renderer::setHolographicWavefrontSettings` / `holographicWavefrontSettings()` — stored
  on renderer.
- `RendererSettings::enableHolographicWavefront` — global gate flag (default `false`).
- `FrameStats::holographicWavefrontActive` — `true` when wavefront encode dispatch ran.
- `FrameStats::holographicWavefrontSliceCount` — depth slices encoded this frame.
- `Renderer::render()`: after light-field display pass, when enabled, dispatches one
  8×8-tile compute pass per depth slice for amplitude + phase pupil-plane propagation.
- New test file `tests/kernel/test_HolographicWavefront.cpp` — 8 Null-backend tests.

---

## [v0.22] — 2026-06-02

### Graphics Kernel

#### Neural Radiance Fields (NeRF) Rendering — Track 1 (Month 79)
- `NeRFSettings` struct — `enabled`, `marchSteps` (64), `networkLayers` (8),
  `posEncodingFreqs` (10).
- `Renderer::setNeRFSettings` / `neRFSettings()` — stored on renderer.
- `RendererSettings::enableNeRF` — global gate flag (default `false`).
- `FrameStats::neRFActive` — `true` when NeRF ray march dispatch ran.
- `FrameStats::neRFMarchSteps` — march steps evaluated this frame.
- `Renderer::render()`: after composite, when enabled, dispatches one 8×8-tile compute
  pass per march step; evaluates MLP-based radiance field for novel-view synthesis.
- New test file `tests/kernel/test_NeRF.cpp` — 8 Null-backend tests.

#### Gaussian Splatting Spectral Extensions — Track 2 (Month 80)
- `GaussianSplatSpectralSettings` struct — `enabled`, `spectralCoefficients` (9),
  `spectralBands` (8).
- `Renderer::setGaussianSplatSpectralSettings` / `gaussianSplatSpectralSettings()` — stored
  on renderer.
- `RendererSettings::enableGaussianSplatSpectral` — global gate flag (default `false`).
- `FrameStats::gaussianSplatSpectralActive` — `true` when spectral splat dispatch ran.
- `FrameStats::gaussianSplatSpectralBandCount` — spectral bands evaluated per splat.
- `Renderer::render()`: after Gaussian splat pass, when enabled, dispatches per-band
  spectral coefficient expansion for spectrally-accurate multi-spectral splatting.
- New test file `tests/kernel/test_GaussianSplatSpectral.cpp` — 8 Null-backend tests.

#### Light-Field Display Output — Track 3 (Month 81)
- `LightFieldDisplaySettings` struct — `enabled`, `viewColumns` (8), `viewRows` (4),
  `displayType` (Lenticular).
- `LightFieldDisplayType` enum — `Lenticular`, `Holographic`.
- `Renderer::setLightFieldDisplaySettings` / `lightFieldDisplaySettings()` — stored on
  renderer.
- `RendererSettings::enableLightFieldDisplay` — global gate flag (default `false`).
- `FrameStats::lightFieldDisplayActive` — `true` when view-tile encode dispatch ran.
- `FrameStats::lightFieldDisplayViewCount` — sub-views encoded this frame
  (`viewColumns × viewRows`).
- `Renderer::render()`: after plenoptic pass, when enabled, dispatches one 8×8-tile compute
  pass per sub-view tile; encodes plenoptic capture into per-view tile layout for
  glasses-free 3D display panels.
- New test file `tests/kernel/test_LightFieldDisplay.cpp` — 8 Null-backend tests.

---

## [v0.21] — 2026-06-02

### Graphics Kernel

#### Plenoptic / Light-Field Rendering — Track 1 (Month 76)
- `PlenopticSettings` struct — `enabled`, `angularResolution` (8), `spatialResolution` (512),
  `refocusDepth` (1.0).
- `Renderer::setPlenopticSettings` / `plenopticSettings()` — stored on renderer.
- `RendererSettings::enablePlenoptic` — global gate flag (default `false`).
- `FrameStats::plenopticActive` — `true` when plenoptic capture dispatch ran.
- `FrameStats::plenopticRayCount` — `width × height × angularResolution²` rays.
- `Renderer::render()`: after composite, when enabled, dispatches one 8×8-tile compute
  pass per angular resolution step; captures the full 4D light field over the grid.
- New test file `tests/kernel/test_Plenoptic.cpp` — 8 Null-backend tests.

#### Coherent Wave Optics — Track 2 (Month 77)
- `WaveOpticsSettings` struct — `enabled`, `diffractionEnabled` (true),
  `interferenceEnabled` (true), `apertureSize` (0.01).
- `Renderer::setWaveOpticsSettings` / `waveOpticsSettings()` — stored on renderer.
- `RendererSettings::enableWaveOptics` — global gate flag (default `false`).
- `FrameStats::waveOpticsActive` — `true` when wave optics passes ran.
- `FrameStats::waveOpticsPassCount` — Fourier passes fired (3 diffraction + 2 interference).
- `Renderer::render()`: after camera lens effects, when enabled, dispatches FFT forward +
  diffraction kernel + IFFT (diffraction) and thin-film accumulate + composite (interference).
- New test file `tests/kernel/test_WaveOptics.cpp` — 8 Null-backend tests.

#### Spectral Participating Media — Track 3 (Month 78)
- `SpectralMediaSettings` struct — `enabled`, `spectralBands` (8), `extinctionScale` (1.0),
  `scatteringScale` (1.0).
- `Renderer::setSpectralMediaSettings` / `spectralMediaSettings()` — stored on renderer.
- `RendererSettings::enableSpectralMedia` — global gate flag (default `false`).
- `FrameStats::spectralMediaActive` — `true` when spectral media dispatch ran.
- `FrameStats::spectralMediaBandCount` — wavelength bands integrated through media.
- `Renderer::render()`: after volumetric pass, when enabled, dispatches one froxel
  compute pass per spectral band for wavelength-dependent extinction and scattering.
- New test file `tests/kernel/test_SpectralMedia.cpp` — 8 Null-backend tests.

---

## [v0.20] — 2026-06-02

### Graphics Kernel

#### Mueller Matrix BSDF — Track 1 (Month 73)
- `MuellerBSDFSettings` struct — `enabled`, `matrixOrder` (4), `trackDepolarisation` (true).
- `Renderer::setMuellerBSDFSettings` / `muellerBSDFSettings()` — stored on renderer.
- `RendererSettings::enableMuellerBSDF` — global gate flag (default `false`).
- `FrameStats::muellerBSDFActive` — `true` when Mueller matrix dispatch ran.
- `FrameStats::muellerBSDFEvalCount` — `width × height × matrixOrder` evaluations.
- `Renderer::render()`: after polarisation pass, when enabled, dispatches one 8×8-tile
  compute pass per matrix order row (capped at 4).
- New test file `tests/kernel/test_MuellerBSDF.cpp` — 8 Null-backend tests.

#### Time-Resolved Phosphorescence Decay — Track 2 (Month 74)
- `PhosphorescenceDecaySettings` struct — `enabled`, `decayFrames` (30),
  `decayExponent` (2.0), `accumulate` (true).
- `Renderer::setPhosphorescenceDecaySettings` / `phosphorescenceDecaySettings()` — stored on renderer.
- `RendererSettings::enablePhosphorescenceDecay` — global gate flag (default `false`).
- `FrameStats::phosphorescenceDecayActive` — `true` when decay accumulation ran.
- `FrameStats::phosphorescenceDecayFrames` — configured decay frame window.
- `Renderer::render()`: after fluorescence pass, when enabled, dispatches emission
  accumulation pass then exponential decay pass.
- New test file `tests/kernel/test_PhosphorescenceDecay.cpp` — 8 Null-backend tests.

#### Hyperspectral IBL — Track 3 (Month 75)
- `HyperspectralIBLSettings` struct — `enabled`, `spectralBands` (8),
  `envMapMipLevels` (6), `importanceSampleCount` (64).
- `Renderer::setHyperspectralIBLSettings` / `hyperspectralIBLSettings()` — stored on renderer.
- `RendererSettings::enableHyperspectralIBL` — global gate flag (default `false`).
- `FrameStats::hyperspectralIBLActive` — `true` when spectral env dispatch ran.
- `FrameStats::hyperspectralIBLBandCount` — spectral bands sampled this frame.
- `Renderer::render()`: after the IBL pass, when enabled, dispatches one 8×8-tile
  compute pass per spectral band sampling the spectral radiance environment map.
- New test file `tests/kernel/test_HyperspectralIBL.cpp` — 8 Null-backend tests.

---

## [v0.19] — 2026-06-02

### Graphics Kernel

#### Polarisation Rendering — Track 1 (Month 70)
- `PolarisationSettings` struct — `enabled`, `stokesComponents` (4), `trackCircular` (true).
- `Renderer::setPolarisationSettings` / `polarisationSettings()` — stored on renderer.
- `RendererSettings::enablePolarisation` — global gate flag (default `false`).
- `FrameStats::polarisationActive` — `true` when Stokes vector dispatch ran.
- `FrameStats::polarisationRayCount` — `width × height × stokesComponents` rays.
- `Renderer::render()`: after multi-spectral pass, when enabled, dispatches one 8×8-tile
  compute pass per active Stokes component (capped at 4).
- New test file `tests/kernel/test_Polarisation.cpp` — 8 Null-backend tests.

#### Fluorescence / Phosphorescence Simulation — Track 2 (Month 71)
- `FluorescenceSettings` struct — `enabled`, `fluorescenceScale` (1.0),
  `phosphorescenceDecay` (0.1), `emissionBands` (4).
- `Renderer::setFluorescenceSettings` / `fluorescenceSettings()` — stored on renderer.
- `RendererSettings::enableFluorescence` — global gate flag (default `false`).
- `FrameStats::fluorescenceActive` — `true` when re-emission pass ran.
- `FrameStats::fluorescenceEmissionBands` — emission bands evaluated this frame.
- `Renderer::render()`: after multi-spectral pass, when enabled, dispatches one 8×8-tile
  re-emission matrix application pass.
- New test file `tests/kernel/test_Fluorescence.cpp` — 8 Null-backend tests.

#### Spectral Upsampling from RGB Assets — Track 3 (Month 72)
- `SpectralUpsamplingMethod` enum — `Smits`, `Sigmoid`.
- `SpectralUpsamplingSettings` struct — `enabled`, `method` (Smits), `upsamplingBands` (8).
- `Renderer::setSpectralUpsamplingSettings` / `spectralUpsamplingSettings()` — stored on renderer.
- `RendererSettings::enableSpectralUpsampling` — global gate flag (default `false`).
- `FrameStats::spectralUpsamplingActive` — `true` when RGB→spectral upsampling ran.
- `FrameStats::spectralUpsamplingMethod` — upsampling method used this frame.
- `Renderer::render()`: before multi-spectral pass, when enabled, dispatches one 8×8-tile
  RGB-to-spectral upsampling pass over the texture atlas.
- New test file `tests/kernel/test_SpectralUpsampling.cpp` — 8 Null-backend tests.

---

## [v0.18] — 2026-06-02

### Graphics Kernel

#### Multi-Spectral Rendering — Track 1 (Month 67)
- `MultiSpectralSettings` struct — `enabled`, `wavelengthBands` (8), `minWavelengthNm` (380.0),
  `maxWavelengthNm` (780.0), `samplesPerBand` (2).
- `Renderer::setMultiSpectralSettings` / `multiSpectralSettings()` — stored on renderer.
- `RendererSettings::enableMultiSpectral` — global gate flag (default `false`).
- `FrameStats::multiSpectralActive` — `true` when multi-spectral dispatch ran.
- `FrameStats::multiSpectralBandCount` — wavelength bands evaluated this frame.
- `Renderer::render()`: after spectral dispersion, when enabled, dispatches one 8×8-tile
  compute pass per `wavelengthBands × samplesPerBand` for full N-wavelength path tracing.
- New test file `tests/kernel/test_MultiSpectral.cpp` — 8 Null-backend tests.

#### Bidirectional Path Tracing (BDPT) — Track 2 (Month 68)
- `BDPTSettings` struct — `enabled`, `lightPathLength` (4), `eyePathLength` (4), `mis` (true).
- `Renderer::setBDPTSettings` / `bdptSettings()` — stored on renderer.
- `RendererSettings::enableBDPT` — global gate flag (default `false`).
- `FrameStats::bdptActive` — `true` when BDPT dispatch ran.
- `FrameStats::bdptConnectionCount` — `width × height × lightPathLength × eyePathLength`.
- `Renderer::render()`: when enabled, dispatches light sub-path trace, eye sub-path trace,
  and MIS connection passes (3 dispatches total).
- New test file `tests/kernel/test_BDPT.cpp` — 8 Null-backend tests.

#### Histogram-Based Auto White Balance — Track 3 (Month 69)
- `AutoWhiteBalanceMethod` enum — `GrayWorld`, `WhitePatch`.
- `AutoWhiteBalanceSettings` struct — `enabled`, `method` (GrayWorld), `strength` (1.0),
  `adaptationSpeed` (0.5).
- `Renderer::setAutoWhiteBalanceSettings` / `autoWhiteBalanceSettings()` — stored on renderer.
- `RendererSettings::enableAutoWhiteBalance` — global gate flag (default `false`).
- `FrameStats::autoWhiteBalanceActive` — `true` when histogram + correction ran.
- `FrameStats::autoWhiteBalanceMethod` — active correction method this frame.
- `Renderer::render()`: after auto-exposure, when enabled, dispatches chrominance histogram
  reduce pass then correction matrix update pass.
- New test file `tests/kernel/test_AutoWhiteBalance.cpp` — 8 Null-backend tests.

---

## [v0.17] — 2026-06-02

### Graphics Kernel

#### Hero Wavelength Spectral Dispersion — Track 1 (Month 64)
- `SpectralSettings` struct — `enabled`, `wavelengthSamples` (4), `dispersionScale` (1.0),
  `heroWavelengthNm` (550.0).
- `Renderer::setSpectralSettings` / `spectralSettings()` — stored on renderer.
- `RendererSettings::enableSpectral` — global gate flag (default `false`).
- `FrameStats::spectralActive` — `true` when spectral dispersion dispatch ran.
- `FrameStats::spectralWavelengthSamples` — wavelength samples evaluated this frame.
- `Renderer::render()`: after ReSTIR PT, when enabled, dispatches one 8×8-tile compute
  pass per `wavelengthSamples` for hero wavelength path-traced dispersion.
- New test file `tests/kernel/test_SpectralDispersion.cpp` — 8 Null-backend tests.

#### Photon Mapping — Track 2 (Month 65)
- `PhotonMappingSettings` struct — `enabled`, `photonCount` (100000), `maxBounces` (4),
  `gatherRadius` (0.05).
- `Renderer::setPhotonMappingSettings` / `photonMappingSettings()` — stored on renderer.
- `RendererSettings::enablePhotonMapping` — global gate flag (default `false`).
- `FrameStats::photonMappingActive` — `true` when photon emission + gather ran.
- `FrameStats::photonsEmitted` — photons traced from all light sources this frame.
- `Renderer::render()`: after GBuffer, when enabled, dispatches photon emission pass
  (ceil(photonCount/64) groups) followed by 8×8-tile gather pass.
- New test file `tests/kernel/test_PhotonMapping.cpp` — 8 Null-backend tests.

#### Auto-Exposure / Eye Adaptation — Track 3 (Month 66)
- `AutoExposureSettings` struct — `enabled`, `minEV` (-4.0), `maxEV` (4.0),
  `adaptationSpeed` (1.0), `targetLuminance` (0.18).
- `Renderer::setAutoExposureSettings` / `autoExposureSettings()` — stored on renderer.
- `RendererSettings::enableAutoExposure` — global gate flag (default `false`).
- `FrameStats::autoExposureActive` — `true` when histogram + EV adaptation ran.
- `FrameStats::autoExposureEV` — computed exposure value applied this frame.
- `Renderer::render()`: after composite, when enabled, dispatches luminance histogram
  reduce pass then EV adaptation update pass.
- New test file `tests/kernel/test_AutoExposure.cpp` — 8 Null-backend tests.

---

## [v0.16] — 2026-06-01

### Graphics Kernel

#### ReSTIR PT Path Tracing — Track 1 (Month 61)
- `ReSTIRPTSettings` struct — `enabled`, `raysPerPixel` (1), `maxPathLength` (6),
  `russianRouletteDepth` (3), `neeEnabled` (true).
- `Renderer::setReSTIRPTSettings` / `reSTIRPTSettings()` — stored on renderer.
- `RendererSettings::enableReSTIRPT` — global gate flag (default `false`).
- `FrameStats::restirPTActive` — `true` when ReSTIR PT dispatch ran.
- `FrameStats::restirPTPathCount` — `width × height × raysPerPixel` paths traced.
- `Renderer::render()`: after RTGI, when enabled, dispatches one 8×8-tile compute pass
  per `raysPerPixel` for full-spectrum PT with NEE and Russian roulette.
- New test file `tests/kernel/test_ReSTIRPT.cpp` — 8 Null-backend tests.

#### Chromatic Aberration & Film Grain — Track 2 (Month 62)
- `CameraLensSettings` struct — `chromaticAberrationEnabled`, `aberrationStrength` (0.005),
  `filmGrainEnabled`, `grainIntensity` (0.04), `grainSize` (1.5).
- `Renderer::setCameraLensSettings` / `cameraLensSettings()` — stored on renderer.
- `RendererSettings::enableCameraLens` — global gate flag (default `false`).
- `FrameStats::cameraLensActive` — `true` when either sub-pass ran.
- `FrameStats::cameraLensPassCount` — 1 when one effect active, 2 when both.
- `Renderer::render()`: after tone mapping, when enabled, dispatches aberration and/or
  grain passes independently based on their respective flags.
- New test file `tests/kernel/test_CameraLensEffects.cpp` — 9 Null-backend tests.

#### Screen-Space Caustics — Track 3 (Month 63)
- `CausticsSettings` struct — `enabled`, `sampleCount` (32), `intensity` (0.5),
  `searchRadius` (0.1).
- `Renderer::setCausticsSettings` / `causticsSettings()` — stored on renderer.
- `RendererSettings::enableCaustics` — global gate flag (default `false`).
- `FrameStats::causticsActive` — `true` when caustic projection pass ran.
- `FrameStats::causticsSampleCount` — `width × height × sampleCount` photon samples.
- `Renderer::render()`: after GBuffer, when enabled, dispatches photon projection
  from refractive geometry in 8×8 tiles; composited additively before lighting.
- New test file `tests/kernel/test_ScreenSpaceCaustics.cpp` — 8 Null-backend tests.

---

## [v0.15] — 2026-06-01

### Graphics Kernel

#### ReSTIR GI — Track 1 (Month 58)
- `ReSTIRSettings` struct — `enabled`, `spatialReuse` (true), `temporalReuse` (true),
  `reservoirSize` (8), `clampThreshold` (10.0).
- `Renderer::setReSTIRSettings` / `reSTIRSettings()` — stored on renderer.
- `RendererSettings::enableReSTIR` — global gate flag (default `false`).
- `FrameStats::restirActive` — `true` when ReSTIR pass ran.
- `FrameStats::restirReservoirCount` — `width × height × reservoirSize` reservoirs updated.
- `Renderer::render()`: after RTGI gather, when enabled, dispatches temporal reuse
  pass (if `temporalReuse`) then spatial reuse pass (if `spatialReuse`) in 8×8 tiles.
- New test file `tests/kernel/test_ReSTIRGI.cpp` — 8 Null-backend tests.

#### Cascaded VSM Blending — Track 2 (Month 59)
- `VSMSettings::cascadeBlendRange` — cross-fade fraction at cascade boundaries (default 0.1).
- `VSMSettings::perCascadeWeights[4]` — per-cascade contribution weight array (default 1.0 each).
- `FrameStats::vsmBlendedCascadeCount` — cascade boundaries with active cross-fade this frame.
- Extended VSM dispatch: blend pass fired per internal boundary when `cascadeBlendRange > 0`
  and more than one cascade is active.
- New test file `tests/kernel/test_CascadedVSMBlending.cpp` — 8 Null-backend tests.

#### Lens Flare & Anamorphic Streaks — Track 3 (Month 60)
- `LensFlareSettings` struct — `enabled`, `ghostCount` (4), `streakLength` (0.8),
  `threshold` (1.0), `intensity` (0.15).
- `Renderer::setLensFlareSettings` / `lensFlareSettings()` — stored on renderer.
- `RendererSettings::enableLensFlare` — global gate flag (default `false`).
- `FrameStats::lensFlareActive` — `true` when lens flare pass ran.
- `FrameStats::lensFlareGhostCount` — ghost sprites composited this frame.
- `Renderer::render()`: after composite, before tone mapping; dispatches ghost radial
  scatter + anamorphic streak horizontal blur in 8×8 tiles.
- New test file `tests/kernel/test_LensFlare.cpp` — 8 Null-backend tests.

---

## [v0.14] — 2026-06-01

### Graphics Kernel

#### Multi-Bounce RTGI Extension — Track 1 (Month 55)
- `RTGISettings::multiBounceFeedback` — prev-frame irradiance blend weight per bounce (default 0.5).
- `RTGISettings::temporalAlpha` — exponential temporal reprojection blend weight (default 0.1).
- `FrameStats::rtgiBounceCount` — bounces dispatched this frame; equals `maxBounces` when RTGI active.
- Dispatch loop now records `rtgiBounceCount` alongside existing `rtgiRaysDispatched`.
- New test file `tests/kernel/test_MultiBounceRTGI.cpp` — 8 Null-backend tests.

#### Horizon-Based Ambient Occlusion (HBAO+) — Track 2 (Month 56)
- `HBAOSettings` struct — `enabled`, `radius` (0.5), `angleBias` (0.1), `sliceCount` (4), `stepCount` (4).
- `Renderer::setHBAOSettings` / `hbaoSettings()` — stored on renderer.
- `RendererSettings::enableHBAO` — global gate flag (default `false`); independent of legacy `enableAO`.
- `FrameStats::hbaoActive` — `true` when HBAO+ dispatch ran.
- `FrameStats::hbaoSampleCount` — `width × height × sliceCount × stepCount`.
- `Renderer::render()`: after GBuffer, when `enableHBAO && settings.enabled`, dispatches
  sliced horizon-angle integration in 8×8 tiles.
- New test file `tests/kernel/test_HBAO.cpp` — 8 Null-backend tests.

#### Variance Shadow Maps (VSM) — Track 3 (Month 57)
- `VSMSettings` struct — `enabled`, `blurRadius` (2.0), `lightBleedReduction` (0.2), `minVariance` (1e-5).
- `Renderer::setVSMSettings` / `vsmSettings()` — stored on renderer.
- `RendererSettings::enableVSM` — global gate flag (default `false`).
- `FrameStats::vsmActive` — `true` when VSM blur+resolve ran.
- `FrameStats::vsmCascadeCount` — shadow cascades processed through VSM this frame.
- `Renderer::render()`: after shadow pass, when `enableVSM && settings.enabled`, dispatches
  horizontal + vertical Gaussian blur per cascade over the depth/depth² atlas.
- New test file `tests/kernel/test_VarianceShadowMaps.cpp` — 8 Null-backend tests.

---

## [v0.13] — 2026-06-01

### Graphics Kernel

#### Ray-Traced Global Illumination (RTGI) — Track 1 (Month 52)
- `RTGISettings` struct — `enabled`, `raysPerPixel` (2), `maxBounces` (1), `denoised` (true).
- `Renderer::setRTGISettings` / `rtgiSettings()` — stored on renderer.
- `RendererSettings::enableRTGI` — global gate flag (default `false`).
- `FrameStats::rtgiActive` — `true` when RTGI dispatch ran.
- `FrameStats::rtgiRaysDispatched` — `width × height × raysPerPixel` rays this frame.
- `Renderer::render()`: after GBuffer, when `enableRTGI && settings.enabled`, dispatches
  one compute pass per bounce in 8×8 tiles.
- New test file `tests/kernel/test_RayTracedGI.cpp` — 8 Null-backend tests.

#### Atmospheric Scattering — Track 2 (Month 53)
- `AtmosphericScatteringSettings` struct — `enabled`, `rayleighScale` (1.0), `mieScale` (1.0),
  `sunZenithAngle` (45°), `lutSize` (256).
- `Renderer::setAtmosphericScatteringSettings` / `atmosphericScatteringSettings()` — stored on renderer.
- `RendererSettings::enableAtmosphericScattering` — global gate flag (default `false`).
- `FrameStats::atmosphericScatteringActive` — `true` when transmittance LUT dispatch ran.
- `FrameStats::atmosphericLUTSize` — transmittance LUT side length dispatched this frame.
- `Renderer::render()`: before composite, when enabled, dispatches `ceil(lutSize/8)²` compute groups.
- New test file `tests/kernel/test_AtmosphericScattering.cpp` — 8 Null-backend tests.

#### Light Shafts (God Rays) — Track 3 (Month 54)
- `LightShaftSettings` struct — `enabled`, `sampleCount` (64), `decay` (0.97), `exposure` (0.3).
- `Renderer::setLightShaftSettings` / `lightShaftSettings()` — stored on renderer.
- `RendererSettings::enableLightShafts` — global gate flag (default `false`).
- `FrameStats::lightShaftsActive` — `true` when radial blur pass ran.
- `FrameStats::lightShaftSampleCount` — radial samples dispatched this frame.
- `Renderer::render()`: after composite, when enabled, dispatches radial blur in 8×8 tiles.
- New test file `tests/kernel/test_LightShafts.cpp` — 8 Null-backend tests.

#### Fixes
- `Renderer.cpp`: anonymous-namespace helpers annotated `[[maybe_unused]]` to suppress
  `-Werror=unused-function` on fresh builds where the scheduler branch is dead-code eliminated.

---

## [v0.12] — 2026-06-01

### Graphics Kernel

#### GPU-Driven Clustered Lighting — Track 1 (Month 49)
- `ClusteredLightingSettings` struct — `enabled`, `clusterDimX` (16), `clusterDimY` (8),
  `clusterDimZ` (24), `maxLightsPerCluster` (128).
- `Renderer::setClusteredLightingSettings` / `clusteredLightingSettings()` — stored on renderer.
- `RendererSettings::enableClusteredLighting` — global gate flag (default `false`).
- `FrameStats::clusteredLightingActive` — `true` when cluster classification dispatch ran.
- `FrameStats::lightClusterCount` — total X×Y×Z clusters classified this frame.
- `FrameStats::clusteredMaxLightsPerCluster` — peak light list capacity per cluster.
- `Renderer::render()`: after GBuffer, when `enableClusteredLighting && settings.enabled`,
  dispatches `clusterDimX × clusterDimY × clusterDimZ` compute groups.
- New test file `tests/kernel/test_ClusteredLighting.cpp` — 8 Null-backend tests.

#### Screen-Space Global Illumination (SSGI) — Track 2 (Month 50)
- `SSGISettings` struct — `enabled`, `rayCount` (4), `rayLength` (2.0), `maxRoughness` (0.8).
- `Renderer::setSSGISettings` / `ssgiSettings()` — stored on renderer.
- `RendererSettings::enableSSGI` — global gate flag (default `false`).
- `FrameStats::ssgiActive` — `true` when SSGI gather dispatch ran.
- `FrameStats::ssgiRayCount` — `width × height × rayCount` rays dispatched this frame.
- `Renderer::render()`: after GBuffer, when `enableSSGI && settings.enabled`, dispatches
  hemisphere ray gather in 8×8 tiles.
- New test file `tests/kernel/test_SSGI.cpp` — 8 Null-backend tests.

#### Decal Rendering Pass — Track 3 (Month 51)
- `DecalSettings` struct — `enabled`, `maxDecals` (256), `atlasResolution` (2048).
- `Renderer::setDecalSettings` / `decalSettings()` — stored on renderer.
- `RendererSettings::enableDecals` — global gate flag (default `false`).
- `FrameStats::decalsActive` — `true` when decal projection pass ran.
- `FrameStats::decalCount` — decals projected this frame (capped at `maxDecals`).
- `Renderer::render()`: after GBuffer, when `enableDecals && settings.enabled`, dispatches
  oriented bounding box decal projection into GBuffer channels in 8×8 tiles.
- New test file `tests/kernel/test_DecalRendering.cpp` — 8 Null-backend tests.

---

## [v0.11] — 2026-06-01

### Graphics Kernel

#### Subsurface Scattering (SSS) — Track 1 (Month 46)
- `SSSSettings` struct — `enabled`, `scatterRadius` (1.0), `profileCount` (1), `blurPasses` (2).
- `Renderer::setSSSSettings` / `sssSettings()` — stored on renderer.
- `RendererSettings::enableSSS` — global gate flag (default `false`).
- `FrameStats::sssActive` — `true` when SSS blur dispatch ran.
- `FrameStats::sssBlurPasses` — total separable passes (horizontal + vertical per pair).
- `Renderer::render()`: after GBuffer, when `enableSSS && settings.enabled`, dispatches
  separable Gaussian blur in 8×8 tiles; `blurPasses * 2` dispatches per frame.
- New test file `tests/kernel/test_SubsurfaceScattering.cpp` — 8 Null-backend tests.

#### Screen-Space Contact Shadows — Track 2 (Month 47)
- `ContactShadowSettings` struct — `enabled`, `rayLength` (0.5), `sampleCount` (16), `thickness` (0.05).
- `Renderer::setContactShadowSettings` / `contactShadowSettings()` — stored on renderer.
- `RendererSettings::enableContactShadows` — global gate flag (default `false`).
- `FrameStats::contactShadowsActive` — `true` when contact shadow march ran.
- `FrameStats::contactShadowRayCount` — `width × height` rays cast this frame.
- `Renderer::render()`: after shadow pass, when `enableContactShadows && settings.enabled`,
  dispatches depth-buffer ray march in 8×8 tiles.
- New test file `tests/kernel/test_ContactShadows.cpp` — 8 Null-backend tests.

#### Tiled Deferred Lighting — Track 3 (Month 48)
- `TiledLightingSettings` struct — `enabled`, `tileSize` (16), `maxLightsPerTile` (256).
- `Renderer::setTiledLightingSettings` / `tiledLightingSettings()` — stored on renderer.
- `RendererSettings::enableTiledLighting` — global gate flag (default `false`).
- `FrameStats::tiledLightingActive` — `true` when tile classification ran.
- `FrameStats::lightTileCount` — total 16×16 tiles classified this frame.
- `FrameStats::maxLightsPerTile` — peak light list capacity per tile.
- `Renderer::render()`: after GBuffer, when `enableTiledLighting && settings.enabled`,
  dispatches tile classification compute; `tilesX × tilesY` groups.
- New test file `tests/kernel/test_TiledDeferredLighting.cpp` — 8 Null-backend tests.

#### Fixes
- `SoftrastExtension.cpp`: fixed `-Werror=misleading-indentation` in AABB loop.

---

## [v0.10] — 2026-06-01

### Graphics Kernel

#### Image-Based Lighting (IBL) — Track 1 (Month 43)
- `IBLSettings` struct — `enabled`, `intensity` (1.0), `diffuseScale` (1.0),
  `specularScale` (1.0), `mipLevels` (6).
- `Renderer::setIBLSettings` / `iblSettings()` — stored on renderer.
- `FrameStats::iblActive` — `true` when IBL compute dispatch ran.
- `FrameStats::iblMipLevels` — prefiltered mip levels sampled this frame.
- `Renderer::render()`: after GBuffer, when `enableIBL && settings.enabled`, dispatches
  compute in 8×8 tiles for specular/diffuse ambient contribution.
- New test file `tests/kernel/test_IBLighting.cpp` — 8 Null-backend tests.

#### Order-Independent Transparency (OIT) — Track 2 (Month 44)
- `OITSettings` struct — `enabled`, `maxLayers` (8), `weightScale` (1.0).
- `Renderer::setOITSettings` / `oitSettings()` — stored on renderer.
- `FrameStats::oitActive` — `true` when OIT resolve pass ran.
- `FrameStats::oitFragmentCount` — `width × height × maxLayers` fragments accumulated.
- `Renderer::render()`: after opaque composite, when `enableOIT && settings.enabled`,
  dispatches weighted-blend OIT resolve in 8×8 tiles.
- New test file `tests/kernel/test_OITransparency.cpp` — 8 Null-backend tests.

#### AMD FSR 4 Upscaler — Track 3 (Month 45)
- `NeuralBackend::FSR4 = 7` — stable enum value; extends existing NeuralBackend chain.
- `UpscalerBackend::FSR4 = 5`, `DenoiserBackend::FSR4` — matching backend identifiers.
- `FSR4Plugin` — header + implementation; same interface as FSR3Plugin; probes
  `ffxFsr4Upscaler*` entry points from the FidelityFX 4 SDK shared library.
- `NeuralRendererFactory::create(FSR4)` — FSR4 probe; falls back to FSR3 then
  deterministic baseline on SDK-absent Null backend.
- Auto-select chain extended: FSR4 probed before FSR3 in `createNeuralRenderer`.
- `perf_smoke` `--neural fsr4` flag wired.
- New test file `tests/kernel/test_FSR4Upscaler.cpp` — 7 Null-backend tests.

---

## [v0.9] — 2026-06-01

### Graphics Kernel

#### Depth of Field — Track 1 (Month 40)
- `DoFSettings` struct — `focalDistance` (10), `focalRange` (5), `maxCoC` (0.05),
  `sampleRadius` (8).
- `Renderer::setDoFSettings` / `dofSettings()` — stored on renderer.
- `FrameStats::dofActive` — `true` when DoF compute pass ran.
- `FrameStats::dofSampleCount` — `width × height × sampleRadius` this frame.
- `Renderer::render()`: after composite, when `enableDoF`, dispatches compute in
  8×8 tiles over the full render target.
- New test file `tests/kernel/test_DepthOfField.cpp` — 8 Null-backend tests.

#### Motion Blur — Track 2 (Month 41)
- `MotionBlurSettings` struct — `shutterAngle` (180°), `sampleCount` (8),
  `maxBlurRadius` (32 px).
- `Renderer::setMotionBlurSettings` / `motionBlurSettings()` — stored on renderer.
- `FrameStats::motionBlurActive` — `true` when motion blur dispatch ran.
- `FrameStats::motionBlurSampleCount` — `width × height × sampleCount` this frame.
- `Renderer::render()`: after DoF, when `enableMotionBlur`, dispatches compute
  reading the GBuffer velocity buffer.
- New test file `tests/kernel/test_MotionBlur.cpp` — 8 Null-backend tests.

#### Tone Mapping / HDR — Track 3 (Month 42)
- `ToneMappingOperator` enum — `Linear=0`, `Reinhard=1`, `ACES=2` (stable values).
- `ToneMappingSettings` struct — `exposure` (1.0), `whitePoint` (1.0),
  `operator_` (ACES).
- `Renderer::setToneMappingSettings` / `toneMappingSettings()` — stored on renderer.
- `FrameStats::tonemapActive` — `true` when tonemap pass ran.
- `FrameStats::tonemapOperator` — enum value used this frame.
- `Renderer::render()`: final pass before present, when `enableToneMapping`, dispatches
  exposure + curve compute in 8×8 tiles.
- New test file `tests/kernel/test_ToneMapping.cpp` — 8 Null-backend tests.

---

## [v0.8] — 2026-06-01

### Graphics Kernel

#### Screen-Space Ambient Occlusion — Track 1 (Month 37)
- `AOSettings` struct — `radius` (0.5), `bias` (0.025), `sampleCount` (16), `blurPasses` (2).
- `Renderer::setAOSettings` / `aoSettings()` — stored on renderer.
- `FrameStats::aoActive` — `true` when SSAO dispatch ran.
- `FrameStats::aoSampleCount` — `width × height × sampleCount` this frame.
- `Renderer::render()`: after GBuffer pass, when `settings.enableAO`, dispatches compute
  in 8×8 tiles over the full render target.
- New test file `tests/kernel/test_ScreenSpaceAO.cpp` — 8 Null-backend tests.

#### Screen-Space Reflections — Track 2 (Month 38)
- `SSRSettings` struct — `maxRaySteps` (64), `stepSize` (0.1), `thickness` (0.05),
  `fadeDistance` (10.0).
- `Renderer::setSSRSettings` / `ssrSettings()` — stored on renderer.
- `FrameStats::ssrActive` — `true` when SSR dispatch ran.
- `FrameStats::ssrRayCount` — `width × height` rays dispatched this frame.
- `Renderer::render()`: after GBuffer pass, when `settings.enableSSR`, dispatches compute
  in 8×8 tiles (before composite).
- New test file `tests/kernel/test_ScreenSpaceReflections.cpp` — 8 Null-backend tests.

#### Bloom Post-Process — Track 3 (Month 39)
- `BloomSettings` struct — `threshold` (1.0), `intensity` (0.04), `radius` (0.85),
  `passes` (5).
- `Renderer::setBloomSettings` / `bloomSettings()` — stored on renderer.
- `FrameStats::bloomActive` — `true` when bloom dispatch chain ran.
- `FrameStats::bloomPassCount` — `passes × 2` (downsample + upsample) this frame.
- `Renderer::render()`: after composite, when `settings.enableBloom`, dispatches
  `passes × 2` compute passes per mip level (Kawase downsample + bicubic upsample).
- New test file `tests/kernel/test_BloomPostProcess.cpp` — 8 Null-backend tests.

---

## [v0.7] — 2026-06-01

### Graphics Kernel

#### MSAA Resolve — Track 3 (Month 36)
- `DeviceCapabilities::maxMsaaSamples` — new cap field; Null backend reports `1` (no MSAA).
- `RendererSettings::msaaSamples` — requested MSAA sample count; clamped to
  `caps().maxMsaaSamples` each frame; `1` = disabled (default).
- `FrameStats::msaaSamples` — actual sample count used this frame (after cap clamp).
- `ICommandBuffer::resolveTexture(src, dst)` — default virtual no-op; Vulkan override
  issues `vkCmdResolveImage` when src sample count > 1.
- `Renderer::render()` stats reset block: `msaaSamples` populated as
  `min(settings.msaaSamples, caps().maxMsaaSamples)`.
- New test file `tests/kernel/test_MSAAResolve.cpp` — 7 Null-backend tests covering
  caps reporting, settings round-trip, stat clamping, and `resolveTexture` no-op.

#### Volumetric Lighting Pass — Track 2 (Month 35)
- `VolumetricSettings` struct — `enabled`, `fogDensity`, `scatteringCoeff`,
  `extinctionCoeff`, `froxelSlices`, `froxelResolutionDivisor`.
- `Renderer::setVolumetricSettings` / `volumetricSettings()` — stored on renderer.
- `FrameStats::volumetricActive` — `true` when the froxel compute pass ran.
- `FrameStats::volumetricFroxelCount` — `froxelW × froxelH × froxelSlices` total cells.
- `Renderer::render()`: after shadow pass, when `settings.enabled`, dispatches a compute
  pass (`cmd.dispatch(groupsX, groupsY, froxelSlices)`) for the froxel integration.
- New test file `tests/kernel/test_VolumetricLighting.cpp` — 8 Null-backend tests.

#### Variable-Rate Shading — Track 1 (Month 34)
- `ShadingRate` enum in `CommandBuffer.h` — `Rate1x1` (0) through `Rate4x4` (5); stable
  values for API-freeze purposes.
- `ICommandBuffer::setFragmentShadingRate(ShadingRate)` — default virtual no-op;
  Vulkan override issues `vkCmdSetFragmentShadingRateKHR`.
- `RendererSettings::enableVRS` — opt-in flag (default `false`); ignored when
  `caps().variableRateShading == false`.
- `RendererSettings::defaultShadingRate` — coarse rate applied to geometry pass
  (default `Rate2x2`).
- `FrameStats::vrsActive` — `true` when `setFragmentShadingRate` fired this frame.
- `Renderer::render()`: before geometry `beginRenderPass`, when
  `enableVRS && caps().variableRateShading`, calls `setFragmentShadingRate(defaultShadingRate)`
  and sets `vrsActive = true`; resets to `Rate1x1` after `endRenderPass`.
- New test file `tests/kernel/test_VariableRateShading.cpp` — 8 Null-backend tests.

---

## [v0.6] — 2026-06-01

### Graphics Kernel

#### RT Shader Binding Tables — Track 3 (Month 33)
- `HitGroup` struct — `closestHit` + optional `anyHit` + optional `intersection` shader handles.
- `ShaderBindingTableDesc` struct — `rayGenShader`, `missShaders[]`, `hitGroups[]`, owning
  `PipelineHandle`; passed to `createShaderBindingTable`.
- `IDevice::createShaderBindingTable(ShaderBindingTableDesc)` — default virtual packs groups
  into a stub `SBTHandle` via the existing `allocateSBT`; Vulkan backend populates
  `VkStridedDeviceAddressRegionKHR` records. Null backend returns a valid stub always.
- `ICommandBuffer::traceRaysWithSBT(SBTHandle, w, h, d)` — default virtual falls back to
  `traceRays(w, h, d)` when the SBT handle is invalid; Vulkan override issues
  `vkCmdTraceRaysKHR` with the four region pointers from the SBT buffer.
- `Renderer::setShaderBindingTable(SBTHandle)` / `shaderBindingTable()` — non-owning SBT
  slot; when valid, `render()` RT block issues `traceRaysWithSBT` instead of bare `traceRays`.
- New test file `tests/kernel/test_RTShaderBindingTable.cpp` — 8 Null-backend tests:
  `createShaderBindingTable` handle validity, pipeline-attached desc no-crash, distinct
  handles, default slot invalid, round-trip set/get, slot clear, render no-crash with SBT,
  and raster stats unaffected by SBT slot.

#### NGX Evaluation Parameter Wiring — Track 2 (Month 32)
- `DLSSPlugin`: `m_featureHandle` and `m_parametersHandle` slots; `AllocParameters`,
  `DestroyParameters`, `SetParameterULL`, `SetParameterF`, `SetParameterVoidP` entry
  points loaded alongside existing pfns.
- `DLSSPlugin::initNGX()` extended: on successful NGX Init, calls `AllocParameters` to
  obtain the per-evaluation parameter block.
- `DLSSPlugin::createFeatureHandle(VkCommandBuffer)` — new private method; calls
  `NVSDK_NGX_VULKAN_CreateFeature` with `featureId=1` (DLSS4) or `featureId=1000` (DLSS-RR)
  depending on `m_rrMode`; result stored in `m_featureHandle`.
- `DLSSPlugin::evaluateFeature(VkCommandBuffer, params)` — thin wrapper around
  `NVSDK_NGX_VULKAN_EvaluateFeature`; no-op when feature or parameter handle is null.
- `DLSSPlugin::upscale()` wired: populates NGX input parameters (color, depth,
  motion vectors, output, render/output resolution, jitter, reset) then calls
  `evaluateFeature`. Falls back to passthrough when feature handle acquisition fails.
- `DLSSPlugin::denoise()` wired: populates color, albedo, normal, motion vectors,
  exposure scale; dispatches via `evaluateFeature`. RR mode uses the same path — the
  correct feature ID was encoded at `createFeatureHandle` time.
- Destructor updated: releases `m_featureHandle`, `m_parametersHandle` before
  `m_ngxHandle` and SDK shutdown.

#### FSR 3 Upscaler Integration — Track 1 (Month 31)
- `NeuralBackend::FSR3` — new enum value (5, API-stable); selects AMD FidelityFX SR 3
  upscaler; falls back to `DeterministicFallbackNeuralRenderer` when SDK is absent.
- `FSR3Plugin` — new `INeuralRenderer` implementation; wraps the FidelityFX SDK via
  `dlopen`/`LoadLibrary`; `available()` true when `ffxFsr3UpscalerContextCreate` resolves.
  `activeUpscaler()` returns `UpscalerBackend::FSR3`; `activeDenoiser()` returns
  `DenoiserBackend::FSR3`. Full `ffxFsr3UpscalerContextDispatch` parameter wiring
  deferred to the Vulkan FFX backend integration milestone.
- `createNeuralRenderer()` `preferFSR` parameter now active — inserts the FSR3 probe
  after XeSS and before the OIDN fallback in the auto-select priority chain.
- `NeuralRendererFactory::create(FSR3, device)` — new factory case; always non-null.
- `perf_smoke` gains `--neural-backend fsr3` / `fsr` token in `parseNeuralBackend`.
- New test file `tests/kernel/test_FSR3Upscaler.cpp` — 7 Null-backend tests covering
  enum distinctness, factory non-null guarantee, SDK-absent fallback, renderer attach,
  upscaling-active gate on fallback, and stable enum value regression guard.

---

## [v0.5] — 2026-06-01

### Graphics Kernel

#### DLSS Ray Reconstruction — Track 3 (Month 30)
- `DenoiserBackend::DLSS_RR` — new enum value, distinct from `DLSS4`; identifies the
  NVIDIA DLSS Ray Reconstruction denoiser path (`NVSDK_NGX_Feature_RayReconstruction`).
- `NeuralBackend::DLSS_RR` — new factory selection enum value; inserts between `DLSS4`
  and `XeSS` in the ordering (value 2, stable for API-freeze purposes).
- `DLSSPlugin` updated: constructor gains `bool rrMode = false` parameter; `activeDenoiser()`
  returns `DLSS_RR` when `m_rrMode && m_ngxAvailable`; `denoise()` branches to the RR
  code path (passthrough until full NGX parameter wiring in the Vulkan RT milestone).
- `NeuralRendererFactory::create(DLSS_RR, device)` — new factory case; constructs a
  `DLSSPlugin(rrMode=true)` when `NEXUS_BACKEND_VULKAN + NEXUS_ENABLE_DLSS` are defined
  and NGX initialises; falls back to `DeterministicFallbackNeuralRenderer` otherwise.
- `perf_smoke` gains `--neural-backend dlss-rr` / `dlss_rr` token in `parseNeuralBackend`.
- New test file `tests/kernel/test_DLSSRayReconstruction.cpp` — 6 Null-backend tests:
  enum distinctness, factory non-null, SDK-absent fallback, renderer attach no-crash,
  denoising-active gate on fallback, and stable enum value regression guard.

#### Ray-Generation Shader Integration — Track 2 (Month 29)
- `DescriptorType::AccelerationStructure` — new enum value (6) for RT TLAS/BLAS bindings;
  `DescriptorBindingDesc::accelStruct` field added alongside existing buffer/texture/sampler.
- `Renderer::setSceneTLAS(AccelStructHandle)` / `sceneTLAS()` — non-owning TLAS slot;
  when valid, the renderer allocates an RT descriptor set at set=0 binding=0 before
  `traceRays` and defers its release to the end of the current frame slot.
- `Renderer::render()` RT dispatch block updated: when `sceneTLAS().valid()`, binds an
  `AccelerationStructure` descriptor set on the command buffer before `cmd.traceRays()`.
  On the Null backend `allocateDescriptorSet` is a no-op, so this path is always safe.
- `test_VulkanRTDispatch.cpp` extended with `TraceRaysWithSceneTLASOnTier1` (10th test):
  builds a single-triangle scene, calls `buildAccelStructs`, attaches the TLAS via
  `setSceneTLAS`, renders one frame, and asserts `rtReflectionsActive == true` and
  `tlasInstanceCount > 0` on Tier-1 RT hardware. Skips cleanly in headless CI.

#### Mesh Shader Production Path — Track 4 (Month 28)
- `RendererSettings::enableMeshShaders` — new flag (default `true`); when false, the renderer
  skips `drawMeshTasks` dispatch even if the device reports `meshShaders == true`.
- `FrameStats::meshShaderDrawCalls` — count of draw calls dispatched via `drawMeshTasks` this
  frame (geometry + shadow passes combined). Reset to 0 at the start of every frame.
- `Renderer::render()` geometry and shadow paths: mesh dispatch now gated on
  `m_settings.enableMeshShaders && m_ctx.caps().meshShaders`; each firing increments
  `m_stats.meshShaderDrawCalls` via per-call-site locals aggregated after submission.
- New test file `tests/kernel/test_MeshShaderProductionPath.cpp` — 8 Null-backend tests:
  default flag state, round-trip flag set/clear, no dispatch without caps, no dispatch with
  flag disabled, draw-call isolation from raster counter, per-frame reset, and pipeline-slot
  API (`setFallbackMeshPipeline`, `setShadowMeshPipeline`) smoke on Null backend.

#### Scene BVH Integration — Track 1 (Month 27)
- `MeshRef::vertexCount` — new field (was missing alongside `indexCount`); required by
  `buildBLAS` to correctly size the acceleration structure on Vulkan hardware.
- `SceneGraph::buildAccelStructs(IDevice&)` — iterates all mesh nodes, builds a BLAS for
  each node that has vertex+index buffers but no BLAS yet, then calls `rebuildTLAS`.
  Returns the count of new BLASes built. Idempotent: nodes with an existing BLAS are skipped.
- `FrameStats::tlasInstanceCount` — count of scene nodes with a valid BLAS this frame;
  populated in the RT dispatch block when `rtReflectionsActive` fires.
- `Renderer::render()` RT block extended: traverses `scene` to count BLAS-equipped nodes
  and writes the result to `m_stats.tlasInstanceCount` when RT dispatch runs.
- New test file `tests/kernel/test_SceneBVH.cpp` — 8 Null-backend tests covering empty
  scene, single-node BLAS build, TLAS validity after build, idempotent second call, multi-
  node build count, `tlasInstanceCount` gate behavior, and `clearAndDestroyTLAS`.

#### v0.4 Release + RT Pipeline Unit Tests (Month 26)
- Project version bumped to `0.4.0`; v0.4 release stamped 2026-05-31.
- Fixed `test_VulkanRTDispatch.cpp`: restored full end-to-end
  `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` test using the correct
  `IDevice::createRayTracingPipeline(RayTracingPipelineDesc)` API (Month 24/25 had
  used the non-existent `createRTPipeline`). Test count: 8 → 9.
- New test file `tests/kernel/test_RTPipelineCreation.cpp` — 7 Null-backend unit tests
  covering RT pipeline creation contract: default handle validity, Null backend create,
  handle round-trip on Renderer, distinct handles per create, minimal/full desc configs,
  and Null backend BLAS/TLAS create.
- `docs/v0.5-planning.md` — planning document for DLSS Ray Reconstruction, full RT
  shader pipeline (BVH scene integration), and mesh shader production path completion.

---

## [v0.4] — 2026-05-31

### Graphics Kernel

#### Hardware Perf Gates + RT Dispatch Test Fix (Month 25)
- Fixed `test_VulkanRTDispatch.cpp`: removed non-existent `createRTPipeline` / `RTPipelineDesc`
  call; replaced with `RTGateFourConditionsOnVulkan` — verifies that on Tier-1 Vulkan hardware
  with `enableRTReflect + HybridRT` mode, `rtReflectionsActive` stays false when no RT pipeline
  is registered (gate condition 4). Test count drops by 0 net (same file, same test count: 8).
- `perf_smoke` extended with three new CLI flags:
  - `--rt` — enables `enableRTReflect + HybridRT` mode in the renderer; exercises the RT
    gate evaluation path every frame.
  - `--neural-backend <auto|dlss|dlss4|xess|oidn|bilinear>` — attaches a
    `NeuralRendererFactory::create()` renderer with upscaling and denoising enabled.
  - `--gpu-ceiling-ms <value>` — asserts `gpuTimeMs < value`; only enforced when
    `gpuTimeMs > 0` (real GPU timing available); exit code 3 on violation.
  - `--neural-overhead-ceiling-ms <value>` — triggers two-phase measurement (with/without
    neural) and asserts the per-frame delta is below the ceiling; exit code 3 on violation.
- `perf_smoke` gracefully skips Vulkan-mode runs (exit 0, prints "SKIPPED: …") when Vulkan
  is unavailable, so the new ctest entries are safe in headless CI.
- `nexus_neural` added to `nexus_kernel_perf_smoke` link libraries.
- New ctest entries: `KernelPerfSmoke.VulkanRT` (120 frames, `--rt`, 33 ms GPU ceiling) and
  `KernelPerfSmoke.DLSSSteadyState` (64 frames, `--neural-backend dlss`, 4 ms overhead ceiling).
- Report now emits `rt_reflections_active`, `upscaling_active`, `denoising_active`, and
  `neural_overhead_ms` fields alongside existing fields.

#### VulkanNeuralRenderer + Live RT Dispatch Tests (Month 24)
- Fixed `DLSSPlugin::activeDenoiser()` / `activeUpscaler()` — now return `DLSS4` when NGX
  initialises successfully (were always returning `None`).
- Fixed `XeSSPlugin::activeUpscaler()` — returns `XeSS` when the XeSS context is live.
- Fixed `OIDNDenoiser::activeDenoiser()` — returns `OIDN_CPU` when `oidnNewDevice` succeeds.
- `NeuralBackend` enum added to `nexus/neural/NeuralRenderer.h` (`Auto`, `DLSS4`, `XeSS`,
  `OIDN_CPU`, `Bilinear`).
- `NeuralRendererFactory::create(NeuralBackend, IDevice&)` — named factory; `Auto` reuses the
  priority chain; explicit backends short-circuit to the requested plugin or fall back to
  `Bilinear`; never returns `nullptr`.
- New test file `tests/kernel/test_VulkanRTDispatch.cpp` — 8 Vulkan-only tests (registered
  under `if(NEXUS_BACKEND_VULKAN)` in `tests/CMakeLists.txt`). All skip cleanly when Vulkan
  is unavailable, `rayTracingTier < 1`, or the relevant SDK is absent. Covers: Vulkan context
  creation, RT capability reporting, `traceRays` dispatch on Tier-1 hardware, factory
  `Auto`/`Bilinear`/`DLSS4`/`XeSS`/`OIDN_CPU` backends, end-to-end `rtReflectionsActive`
  and `upscalingActive` / `denoisingActive` stats.

#### v0.3 Release + M5 Contributor Scale-Up (Month 23)
- Project version bumped to `0.3.0` in CMakeLists.txt; v0.3 release stamped 2026-05-31.
- `docs/api-contract-alpha-summary.md` updated to reflect 93-header manifest (was 39 at alpha
  baseline); full domain ownership table and per-domain delta summary added.
- `docs/testing-strategy.md` updated to 2210-test baseline; scope section expanded to cover
  all M1–M4 test areas including perf smoke and scenario artifact validation.
- `CONTRIBUTING.md` expanded with domain ownership map, API surface rules, and test authoring
  guidance.
- New `docs/contributing-kernel.md` — detailed kernel contribution guide covering Null backend
  parity, public header lifecycle, test authoring patterns, and the render-pass sequence.
- New `docs/v0.4-planning.md` — planning document for live DLSS4/XeSS integration on Vulkan
  RT hardware (VulkanNeuralRenderer, live RT dispatch tests, hardware perf gates).
- `docs/getting-started.md` test-suite table updated with v0.3 totals (2210 tests).

---

## [v0.3] — 2026-05-31

### Graphics Kernel

#### RT/Mesh-Shader Production Path — M4 Complete (Month 22)
- `FrameStats::activeRenderMode` — reflects the render mode actually used after
  capability downgrade (e.g. PathTrace requested on a no-RT backend → Rasterize).
- `Renderer::setRayTracingPipeline(PipelineHandle)` / `rayTracingPipeline()` —
  new API slot for the RT pipeline used in HybridRT and PathTrace modes.
- `FrameStats::rtReflectionsActive` — true when the RT reflections pass ran
  (`enableRTReflect` + HybridRT/PathTrace mode + RT caps + valid RT pipeline).
- `traceRays` dispatch wired into the scheduler render path, gated on all four
  conditions; safely no-ops on Null backend (RT caps absent → downgrade to Rasterize).
- **M4 milestone closed.** All M4 exit criteria are now satisfied.
- New test file `tests/kernel/test_RTProductionPath.cpp` — 13 tests.

#### Async-Compute Upscaler Scheduling + DLSS/XeSS Perf Gate (Month 21)
- `RendererSettings::enableUpscaling` — new flag; when true and a neural renderer
  is attached, `Renderer::render()` calls `INeuralRenderer::upscale()` after the
  denoiser pass.
- `FrameStats::upscalingActive` — true when the upscaler ran this frame.
- `FrameStats::activeUpscaler` — reflects the `UpscalerBackend` reported by the
  attached `INeuralRenderer` (`Bilinear`, `DLSS4`, `XeSS`, `FSR3`, or `None`).
- Denoiser and upscaler can run together in a single frame when both flags are set.
- DLSS/XeSS perf gate: `NeuralDispatchOverheadBelowCeilingOnNullBackend` asserts
  the average per-frame dispatch cost across 64 Null-backend frames stays under
  50 ms, preventing silent scheduling overhead regressions.
- New test file `tests/kernel/test_NeuralUpscaler.cpp` — 12 tests.

#### Async-Compute Denoiser Scheduling (Month 20)
- `Renderer` now accepts a non-owning `INeuralRenderer*` via `setNeuralRenderer`.
- `RendererSettings::enableDenoising` — new flag; when true and a neural renderer
  is attached, `Renderer::render()` calls `INeuralRenderer::denoise()` after the
  composite pass on the async-compute command slot.
- `FrameStats::denoisingActive` — true when the denoiser ran this frame.
- `FrameStats::activeDenoiser` — reflects the backend reported by the attached
  `INeuralRenderer` (`OIDN_CPU`, `DLSS4`, `XeSS`, `FSR3`, or `None`).
- New test file `tests/kernel/test_NeuralDenoiser.cpp` — 11 tests covering
  attach/detach contract, per-frame dispatch, backend reflection, and draw-call
  isolation. All tests use a `StubNeuralRenderer` on the Null backend.

#### CI Smoke Suite Hardening (Month 19)
- Fixed 6 broken softrast scenario scripts (`softrast_multi_object`,
  `softrast_textured_sphere`, `softrast_remesh_render`, `softrast_geometry_ops`,
  `softrast_sculpt_render`, `softrast_parametric_render`) — now produce required
  `summary.json` / `diagnostics.txt` / `deterministic_signature.txt` artifacts.
- Fixed `nexus_kernel_perf_smoke` build failure on environments without the
  Vulkan SDK by gating Vulkan includes behind `#ifdef NEXUS_BACKEND_VULKAN`.
  `KernelPerfSmoke.Null` and `KernelPerfSmoke.Determinism` now pass in CI.

#### TAA Renderer Integration (Month 18)
- `Renderer` now owns a `TemporalAccumulator` instance (`m_taaAccumulator`).
- `Renderer::render()` advances the TAA frame index and applies per-frame subpixel
  camera jitter via `Camera::setJitter` when `RendererSettings::enableTAA` is true.
  Jitter is cleared after each frame via a local jittered camera copy so frustum
  culling is never affected.
- `FrameStats::taaFrameIndex` — new field; reports the TAA accumulator frame counter
  (zero when TAA is disabled).
- New public API on `Renderer`:
  - `setTemporalAccumulationConfig(const TemporalAccumulationConfig&)`
  - `temporalAccumulationConfig() const`
  - `temporalAccumulationState() const`
- New test file `tests/kernel/test_TemporalAccumulation.cpp` — 27 tests covering
  `TemporalAccumulator` unit behaviour and Renderer TAA integration on the Null backend.

#### CPU Frustum-Cull Timing (Month 17)
- `FrameStats::cpuCullTimeMs` — populated from `std::chrono::steady_clock` around
  the `SceneGraph::collectVisible` call in `Renderer::render()`.
- `perf_smoke` now emits `cpu_cull_time_ms=<value>` alongside other per-frame fields.

#### Shadow Pipeline (Month 16–17)
- `FrameStats::shadowDrawCalls` — isolated shadow-pass draw call counter, zero when
  the shadow pipeline is not active or no eligible nodes exist.
- GPU timestamp ring (`FrameTimingLayer`) — 12 tests; `FrameStats::gpuTimeMs` populated.

### API Surface

- `TemporalAccumulationConfig`, `TemporalAccumulationState`, `TemporalAccumulator`
  headers are now part of the tracked API surface manifest.

---

## [v0.2] — released

- Gaussian splat pass (`GaussianSplatPass`) — CPU-side counter path, Null-backend safe.
- Descriptor binder RAII wrappers — `CompositeDescriptorSet`, `MaterialTableDescriptorSet`,
  `ShadowDescriptorSet`.
- Shadow map target (`ShadowMapTarget`) — depth texture + cascade lifecycle.
- GBuffer texture management.
- Render path parity formally de-scoped; scheduler path is authoritative.
- `FrameStats` — initial fields: `drawCalls`, `triangles`, `meshlets`, `splatDrawCalls`,
  `submittedSplats`, `projectedSplats`.
