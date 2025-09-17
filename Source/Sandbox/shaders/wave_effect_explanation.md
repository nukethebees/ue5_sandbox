# HLSL Wave Effect Function Explanation

## Overview
The `GetForcefieldWave` function creates a vertical line that moves horizontally across textures, repeating continuously. The line is brighter than the base color where the wave is present, and the background is dimmed where the wave is not.

## Function Signature
```hlsl
float3 GetForcefieldWave(
    float2 UV,              // Texture coordinates (0.0-1.0)
    float WaveSpeed,        // How fast the wave moves horizontally
    float WaveWidth,        // Thickness of the wave line (as fraction)
    float WaveBrightness,   // Brightness multiplier for wave areas
    float BaseDimming,      // Brightness multiplier for non-wave areas
    float3 MyBaseColor      // Base emissive color to modify
)
```

## Key Concepts Explained

### 1. Wave Position (Repeating Movement)
```hlsl
float wave_position = fmod(time * WaveSpeed, 1.0f);
```
- `time * WaveSpeed`: Distance = speed × time (physics formula)
- `fmod(x, 1.0f)`: Gets the remainder after dividing by 1.0
- **Example**: `fmod(2.7, 1.0) = 0.7`
- **Purpose**: Keeps wave position between 0.0-1.0 to match UV coordinates
- **Effect**: Wave continuously cycles from left edge to right edge and repeats

### 2. Distance Calculation
```hlsl
float distance_from_wave = abs(UV.x - wave_position);
```
- Calculates how far each pixel is from the current wave position
- `abs()` ensures distance is always positive
- Range: 0.0 (at wave center) to 1.0 (maximum distance)

### 3. Wrapped Distance (Seamless Looping)
```hlsl
float wrapped_distance = min(distance_from_wave, 1.0f - distance_from_wave);
```
- **Problem**: Without wrapping, waves "pop" when restarting at left edge
- **Solution**: Consider both direct distance AND wrapped distance
- **Example**: Wave at 0.9, pixel at 0.1
  - Direct distance: `abs(0.1 - 0.9) = 0.8`
  - Wrapped distance: `1.0 - 0.8 = 0.2`
  - Takes minimum: `0.2` (closer to where wave will appear next)
- **Effect**: Creates seamless wave continuity across edges

### 4. Wave Intensity (Creating the Line)
```hlsl
float wave_intensity = 1.0f - smoothstep(0.0f, WaveWidth * 0.5f, wrapped_distance);
```

#### smoothstep Function
`smoothstep(edge0, edge1, x)` creates smooth S-curve transitions:
- **edge0**: Start of transition (output = 0)
- **edge1**: End of transition (output = 1)
- **x**: Input value to evaluate
- **Behavior**:
  - `x <= edge0`: returns 0
  - `x >= edge1`: returns 1
  - Between: smooth curve from 0 to 1

#### In our context:
- `edge0 = 0.0f`: At wave center, smoothstep returns 0
- `edge1 = WaveWidth * 0.5f`: At half wave width away, smoothstep returns 1
- `x = wrapped_distance`: Distance from wave center
- `1.0f - smoothstep(...)`: **Inverts** the result so wave center = 1.0 (bright), edges = 0.0 (dark)

### 5. Brightness Multiplier (Final Effect)
```hlsl
float brightness_multiplier = lerp(BaseDimming, WaveBrightness, wave_intensity);
```
- `lerp(a, b, t)`: Linear interpolation between `a` and `b` using factor `t`
- When `wave_intensity = 0.0` (no wave): uses `BaseDimming` (darker background)
- When `wave_intensity = 1.0` (wave center): uses `WaveBrightness` (brighter line)
- Between: smooth transition

## Usage in Unreal Engine Materials

### Getting UV Coordinates
**Option 1**: TexCoord node connected as input to Custom Expression
**Option 2**: Use built-in `Parameters.TexCoords[0].xy` inside Custom Expression

### Example Call
```hlsl
GetForcefieldWave(
    Parameters.TexCoords[0].xy,  // UV coordinates
    0.2,                         // WaveSpeed (waves per second)
    0.05,                        // WaveWidth (5% of texture width)
    2.0,                         // WaveBrightness (2x brighter)
    0.3,                         // BaseDimming (70% darker background)
    MyBaseColor                  // Base emissive color
)
```

## Visual Result
- Bright vertical line moves smoothly from left to right across texture
- Background is dimmed when wave is not present
- Wave repeats continuously with seamless wrapping
- Anti-aliased edges due to smoothstep function
- Fully time-based animation using game time