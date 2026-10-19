// PC enhancement: ambient occlusion (GTAO) - soft contact shadows in corners, under objects and
// where surfaces meet (display.ini "occlusion_ambiante", 0 = off).
//
// After the opaque scene draws (GameScene::draw3D, before the translucent ones and the 2D), the
// EFB depth is snapshotted with Aurora's resolve_pass, a compute chain (depth MIP prefilter,
// GTAO, spatial denoise) produces a visibility texture, and a fullscreen draw multiplies it over
// the frame. Aurora's public gfx API (aurora/gfx.hpp) does the pass bookkeeping.
//
// Ported from Dusklight's ao_mod (the Twilight Princess PC port), whose WGSL comes from Bevy
// Engine's SSAO (MIT OR Apache-2.0), itself based on Intel XeGTAO (MIT): see pc/src/fx/licenses.
// Differences: no normal buffer (Aurora only has one without MSAA), so normals are rebuilt from
// the depth; no-geometry pixels (sky, cleared depth) are masked by their depth; the projection is
// the game camera's GX one, converted like Aurora does (reversed Z).

#include "Game/Util/CameraUtil.hpp"

#include <aurora/gfx.hpp>
#include <webgpu/webgpu_cpp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

float pc_settings_ssao();

namespace {

using namespace aurora;

constexpr const char* kUniforms = R"WGSL(
struct Uniforms {
    projection: mat4x4f,
    inverse_projection: mat4x4f,
    size: vec2f,        // AO chain size in pixels (MIP 0 of the preprocessed depth)
    inv_size: vec2f,
    depth_scale: vec2f, // input depth snapshot pixels per chain pixel (1 or 2)
    effect_radius: f32, // view-space units
    intensity: f32,
    slice_count: f32,
    samples_per_slice_side: f32,
    debug_view: u32,
    _pad: f32,
}
)WGSL";

// Bevy preprocess_depth.wgsl (see Dusklight's port notes): MIP chain of the depth.
constexpr const char* kPreprocessShader = R"WGSL(
@group(0) @binding(0) var input_depth: texture_2d<f32>;
@group(0) @binding(1) var preprocessed_depth_mip0: texture_storage_2d<r32float, write>;
@group(0) @binding(2) var preprocessed_depth_mip1: texture_storage_2d<r32float, write>;
@group(0) @binding(3) var preprocessed_depth_mip2: texture_storage_2d<r32float, write>;
@group(0) @binding(4) var preprocessed_depth_mip3: texture_storage_2d<r32float, write>;
@group(0) @binding(5) var<uniform> uniforms: Uniforms;
@group(0) @binding(6) var preprocessed_depth_mip3_in: texture_2d<f32>;
@group(0) @binding(7) var preprocessed_depth_mip4: texture_storage_2d<r32float, write>;

fn load_input_depth(pixel_coordinates: vec2<i32>) -> f32 {
    let input_size = vec2<i32>(uniforms.size * uniforms.depth_scale);
    let coordinates = clamp(vec2<i32>(vec2<f32>(pixel_coordinates) * uniforms.depth_scale),
        vec2<i32>(0i), input_size - 1i);
    return textureLoad(input_depth, coordinates, 0i).r;
}

fn weighted_average(depth0: f32, depth1: f32, depth2: f32, depth3: f32) -> f32 {
    let depth_range_scale_factor = 0.75;
    let effect_radius = depth_range_scale_factor * 0.5 * 1.457;
    let falloff_range = 0.615 * effect_radius;
    let falloff_from = effect_radius * (1.0 - 0.615);
    let falloff_mul = -1.0 / falloff_range;
    let falloff_add = falloff_from / falloff_range + 1.0;

    let min_depth = min(min(depth0, depth1), min(depth2, depth3));
    let weight0 = saturate((depth0 - min_depth) * falloff_mul + falloff_add);
    let weight1 = saturate((depth1 - min_depth) * falloff_mul + falloff_add);
    let weight2 = saturate((depth2 - min_depth) * falloff_mul + falloff_add);
    let weight3 = saturate((depth3 - min_depth) * falloff_mul + falloff_add);
    let weight_total = weight0 + weight1 + weight2 + weight3;

    return ((weight0 * depth0) + (weight1 * depth1) + (weight2 * depth2) + (weight3 * depth3)) / weight_total;
}

var<workgroup> previous_mip_depth: array<array<f32, 8>, 8>;

@compute
@workgroup_size(8, 8, 1)
fn preprocess_depth(@builtin(global_invocation_id) global_id: vec3<u32>, @builtin(local_invocation_id) local_id: vec3<u32>) {
    let base_coordinates = vec2<i32>(global_id.xy);

    let pixel_coordinates0 = base_coordinates * 2i;
    let pixel_coordinates1 = pixel_coordinates0 + vec2<i32>(1i, 0i);
    let pixel_coordinates2 = pixel_coordinates0 + vec2<i32>(0i, 1i);
    let pixel_coordinates3 = pixel_coordinates0 + vec2<i32>(1i, 1i);
    let depth0 = load_input_depth(pixel_coordinates0);
    let depth1 = load_input_depth(pixel_coordinates1);
    let depth2 = load_input_depth(pixel_coordinates2);
    let depth3 = load_input_depth(pixel_coordinates3);
    textureStore(preprocessed_depth_mip0, pixel_coordinates0, vec4<f32>(depth0, 0.0, 0.0, 0.0));
    textureStore(preprocessed_depth_mip0, pixel_coordinates1, vec4<f32>(depth1, 0.0, 0.0, 0.0));
    textureStore(preprocessed_depth_mip0, pixel_coordinates2, vec4<f32>(depth2, 0.0, 0.0, 0.0));
    textureStore(preprocessed_depth_mip0, pixel_coordinates3, vec4<f32>(depth3, 0.0, 0.0, 0.0));

    let depth_mip1 = weighted_average(depth0, depth1, depth2, depth3);
    textureStore(preprocessed_depth_mip1, base_coordinates, vec4<f32>(depth_mip1, 0.0, 0.0, 0.0));
    previous_mip_depth[local_id.x][local_id.y] = depth_mip1;

    workgroupBarrier();

    if all(local_id.xy % vec2<u32>(2u) == vec2<u32>(0u)) {
        let mip2_depth0 = previous_mip_depth[local_id.x + 0u][local_id.y + 0u];
        let mip2_depth1 = previous_mip_depth[local_id.x + 1u][local_id.y + 0u];
        let mip2_depth2 = previous_mip_depth[local_id.x + 0u][local_id.y + 1u];
        let mip2_depth3 = previous_mip_depth[local_id.x + 1u][local_id.y + 1u];
        let depth_mip2 = weighted_average(mip2_depth0, mip2_depth1, mip2_depth2, mip2_depth3);
        textureStore(preprocessed_depth_mip2, base_coordinates / 2i, vec4<f32>(depth_mip2, 0.0, 0.0, 0.0));
        previous_mip_depth[local_id.x][local_id.y] = depth_mip2;
    }

    workgroupBarrier();

    if all(local_id.xy % vec2<u32>(4u) == vec2<u32>(0u)) {
        let mip3_depth0 = previous_mip_depth[local_id.x + 0u][local_id.y + 0u];
        let mip3_depth1 = previous_mip_depth[local_id.x + 2u][local_id.y + 0u];
        let mip3_depth2 = previous_mip_depth[local_id.x + 0u][local_id.y + 2u];
        let mip3_depth3 = previous_mip_depth[local_id.x + 2u][local_id.y + 2u];
        let depth_mip3 = weighted_average(mip3_depth0, mip3_depth1, mip3_depth2, mip3_depth3);
        textureStore(preprocessed_depth_mip3, base_coordinates / 4i, vec4<f32>(depth_mip3, 0.0, 0.0, 0.0));
        previous_mip_depth[local_id.x][local_id.y] = depth_mip3;
    }
}

@compute
@workgroup_size(8, 8, 1)
fn downsample_mip4(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let base_coordinates = vec2<i32>(global_id.xy);
    let mip3_size = max(vec2<i32>(textureDimensions(preprocessed_depth_mip3_in)), vec2<i32>(1i));
    let coordinates0 = clamp(base_coordinates * 2i, vec2<i32>(0i), mip3_size - 1i);
    let coordinates1 = clamp(base_coordinates * 2i + vec2<i32>(1i, 0i), vec2<i32>(0i), mip3_size - 1i);
    let coordinates2 = clamp(base_coordinates * 2i + vec2<i32>(0i, 1i), vec2<i32>(0i), mip3_size - 1i);
    let coordinates3 = clamp(base_coordinates * 2i + vec2<i32>(1i, 1i), vec2<i32>(0i), mip3_size - 1i);
    let depth0 = textureLoad(preprocessed_depth_mip3_in, coordinates0, 0i).r;
    let depth1 = textureLoad(preprocessed_depth_mip3_in, coordinates1, 0i).r;
    let depth2 = textureLoad(preprocessed_depth_mip3_in, coordinates2, 0i).r;
    let depth3 = textureLoad(preprocessed_depth_mip3_in, coordinates3, 0i).r;
    let depth_mip4 = weighted_average(depth0, depth1, depth2, depth3);
    textureStore(preprocessed_depth_mip4, base_coordinates, vec4<f32>(depth_mip4, 0.0, 0.0, 0.0));
}
)WGSL";

// Bevy gtao.wgsl, with the normal rebuilt from the depth (no normal buffer under MSAA).
constexpr const char* kGtaoShader = R"WGSL(
@group(0) @binding(0) var preprocessed_depth: texture_2d<f32>;
@group(0) @binding(1) var hilbert_index_lut: texture_2d<u32>;
@group(0) @binding(2) var ambient_occlusion: texture_storage_2d<r32float, write>;
@group(0) @binding(3) var depth_differences: texture_storage_2d<r32uint, write>;
@group(0) @binding(4) var<uniform> uniforms: Uniforms;

const PI: f32 = 3.141592653589793;
const HALF_PI: f32 = 1.5707963267948966;
// Reversed Z: the cleared depth (nothing drawn: sky, space) is 0.
const NO_GEOMETRY_DEPTH: f32 = 0.0000001;

fn fast_sqrt(x: f32) -> f32 {
    return bitcast<f32>(0x1fbd1df5 + (bitcast<i32>(x) >> 1u));
}

fn fast_acos(in_x: f32) -> f32 {
    let x = abs(in_x);
    var res = -0.156583 * x + HALF_PI;
    res *= fast_sqrt(1.0 - x);
    return select(PI - res, res, in_x >= 0.0);
}

fn load_noise(pixel_coordinates: vec2<i32>) -> vec2<f32> {
    let index = textureLoad(hilbert_index_lut, pixel_coordinates % 64, 0).r;
    return fract(0.5 + f32(index) * vec2<f32>(0.75487766624669276005, 0.5698402909980532659114));
}

fn load_depth(pixel_coordinates: vec2<i32>, mip_level: i32) -> f32 {
    let mip_size = max(vec2<i32>(uniforms.size) >> vec2<u32>(u32(mip_level)), vec2<i32>(1i));
    let coordinates = clamp(pixel_coordinates, vec2<i32>(0i), mip_size - 1i);
    return textureLoad(preprocessed_depth, coordinates, mip_level).r;
}

fn calculate_neighboring_depth_differences(pixel_coordinates: vec2<i32>) -> f32 {
    let depth_center = load_depth(pixel_coordinates, 0i);
    let depth_left = load_depth(pixel_coordinates + vec2<i32>(-1i, 0i), 0i);
    let depth_top = load_depth(pixel_coordinates + vec2<i32>(0i, -1i), 0i);
    let depth_bottom = load_depth(pixel_coordinates + vec2<i32>(0i, 1i), 0i);
    let depth_right = load_depth(pixel_coordinates + vec2<i32>(1i, 0i), 0i);

    var edge_info = vec4<f32>(depth_left, depth_right, depth_top, depth_bottom) - depth_center;
    let slope_left_right = (edge_info.y - edge_info.x) * 0.5;
    let slope_top_bottom = (edge_info.w - edge_info.z) * 0.5;
    let edge_info_slope_adjusted = edge_info + vec4<f32>(slope_left_right, -slope_left_right, slope_top_bottom, -slope_top_bottom);
    edge_info = min(abs(edge_info), abs(edge_info_slope_adjusted));
    let bias = 0.25;
    let scale = depth_center * 0.011;
    edge_info = saturate((1.0 + bias) - edge_info / scale);

    let edge_info_packed = vec4<u32>(pack4x8unorm(edge_info), 0u, 0u, 0u);
    textureStore(depth_differences, pixel_coordinates, edge_info_packed);

    return depth_center;
}

fn reconstruct_view_space_position(depth: f32, uv: vec2<f32>) -> vec3<f32> {
    let clip_xy = vec2<f32>(uv.x * 2.0 - 1.0, 1.0 - 2.0 * uv.y);
    let t = uniforms.inverse_projection * vec4<f32>(clip_xy, depth, 1.0);
    let view_xyz = t.xyz / t.w;
    return view_xyz;
}

fn view_position_at(pixel_coordinates: vec2<i32>) -> vec3<f32> {
    let depth = load_depth(pixel_coordinates, 0i);
    let uv = (vec2<f32>(pixel_coordinates) + 0.5) * uniforms.inv_size;
    return reconstruct_view_space_position(depth, uv);
}

// View-space normal from the depth: on each axis, the neighbour closest in depth (so the edges
// of objects don't bend the normal towards the background).
fn reconstruct_normal(pixel_coordinates: vec2<i32>, center: vec3<f32>) -> vec3<f32> {
    let left = view_position_at(pixel_coordinates + vec2<i32>(-1i, 0i));
    let right = view_position_at(pixel_coordinates + vec2<i32>(1i, 0i));
    let top = view_position_at(pixel_coordinates + vec2<i32>(0i, -1i));
    let bottom = view_position_at(pixel_coordinates + vec2<i32>(0i, 1i));
    let dx = select(right - center, center - left, abs(left.z - center.z) < abs(right.z - center.z));
    let dy = select(bottom - center, center - top, abs(top.z - center.z) < abs(bottom.z - center.z));
    // +x on screen is +x in view space, +y on screen (down) is -y: this points at the camera.
    return normalize(cross(dy, dx));
}

fn load_and_reconstruct_view_space_position(uv: vec2<f32>, sample_mip_level: f32) -> vec3<f32> {
    let mip_level = i32(sample_mip_level + 0.5);
    let mip_size = max(vec2<i32>(uniforms.size) >> vec2<u32>(u32(mip_level)), vec2<i32>(1i));
    let depth = load_depth(vec2<i32>(uv * vec2<f32>(mip_size)), mip_level);
    return reconstruct_view_space_position(depth, uv);
}

@compute
@workgroup_size(8, 8, 1)
fn gtao(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let slice_count = uniforms.slice_count;
    let samples_per_slice_side = uniforms.samples_per_slice_side;
    let effect_radius = uniforms.effect_radius;
    let falloff_range = 0.615 * effect_radius;
    let falloff_from = effect_radius * (1.0 - 0.615);
    let falloff_mul = -1.0 / falloff_range;
    let falloff_add = falloff_from / falloff_range + 1.0;

    let pixel_coordinates = vec2<i32>(global_id.xy);
    let uv = (vec2<f32>(pixel_coordinates) + 0.5) * uniforms.inv_size;

    var pixel_depth = calculate_neighboring_depth_differences(pixel_coordinates);
    if pixel_depth < NO_GEOMETRY_DEPTH {
        textureStore(ambient_occlusion, pixel_coordinates, vec4f(1.0));
        return;
    }
    pixel_depth += 0.00001;

    let pixel_position = reconstruct_view_space_position(pixel_depth, uv);
    let pixel_normal = reconstruct_normal(pixel_coordinates, view_position_at(pixel_coordinates));
    let view_vec = normalize(-pixel_position);

    let noise = load_noise(pixel_coordinates);
    let sample_scale = (-0.5 * effect_radius * uniforms.projection[0][0]) / pixel_position.z;

    var visibility = 0.0;
    for (var slice_t = 0.0; slice_t < slice_count; slice_t += 1.0) {
        let slice = slice_t + noise.x;
        let phi = (PI / slice_count) * slice;
        let omega = vec2<f32>(cos(phi), sin(phi));

        let direction = vec3<f32>(omega.xy, 0.0);
        let orthographic_direction = direction - (dot(direction, view_vec) * view_vec);
        let axis = cross(direction, view_vec);
        let projected_normal = pixel_normal - axis * dot(pixel_normal, axis);
        let projected_normal_length = length(projected_normal);

        let sign_norm = sign(dot(orthographic_direction, projected_normal));
        let cos_norm = saturate(dot(projected_normal, view_vec) / projected_normal_length);
        let n = sign_norm * fast_acos(cos_norm);

        let min_cos_horizon_1 = cos(n + HALF_PI);
        let min_cos_horizon_2 = cos(n - HALF_PI);
        var cos_horizon_1 = min_cos_horizon_1;
        var cos_horizon_2 = min_cos_horizon_2;
        let sample_mul = vec2<f32>(omega.x, -omega.y) * sample_scale;
        for (var sample_t = 0.0; sample_t < samples_per_slice_side; sample_t += 1.0) {
            var sample_noise = (slice_t + sample_t * samples_per_slice_side) * 0.6180339887498948482;
            sample_noise = fract(noise.y + sample_noise);

            var s = (sample_t + sample_noise) / samples_per_slice_side;
            s *= s;
            let sample = s * sample_mul;

            let sample_mip_level = clamp(log2(length(sample * uniforms.size)) - 3.3, 0.0, 4.0);
            let sample_position_1 = load_and_reconstruct_view_space_position(uv + sample, sample_mip_level);
            let sample_position_2 = load_and_reconstruct_view_space_position(uv - sample, sample_mip_level);

            let sample_difference_1 = sample_position_1 - pixel_position;
            let sample_difference_2 = sample_position_2 - pixel_position;
            let sample_distance_1 = length(sample_difference_1);
            let sample_distance_2 = length(sample_difference_2);
            var sample_cos_horizon_1 = dot(sample_difference_1 / sample_distance_1, view_vec);
            var sample_cos_horizon_2 = dot(sample_difference_2 / sample_distance_2, view_vec);

            let weight_1 = saturate(sample_distance_1 * falloff_mul + falloff_add);
            let weight_2 = saturate(sample_distance_2 * falloff_mul + falloff_add);
            sample_cos_horizon_1 = mix(min_cos_horizon_1, sample_cos_horizon_1, weight_1);
            sample_cos_horizon_2 = mix(min_cos_horizon_2, sample_cos_horizon_2, weight_2);

            cos_horizon_1 = max(cos_horizon_1, sample_cos_horizon_1);
            cos_horizon_2 = max(cos_horizon_2, sample_cos_horizon_2);
        }

        let horizon_1 = fast_acos(cos_horizon_1);
        let horizon_2 = -fast_acos(cos_horizon_2);
        let v1 = (cos_norm + 2.0 * horizon_1 * sin(n) - cos(2.0 * horizon_1 - n)) / 4.0;
        let v2 = (cos_norm + 2.0 * horizon_2 * sin(n) - cos(2.0 * horizon_2 - n)) / 4.0;
        visibility += projected_normal_length * (v1 + v2);
    }
    visibility /= slice_count;
    visibility = clamp(visibility, 0.03, 1.0);

    textureStore(ambient_occlusion, pixel_coordinates, vec4<f32>(visibility, 0.0, 0.0, 0.0));
}
)WGSL";

// Bevy spatial_denoise.wgsl: 3x3 edge-preserving blur.
constexpr const char* kDenoiseShader = R"WGSL(
@group(0) @binding(0) var ambient_occlusion_noisy: texture_2d<f32>;
@group(0) @binding(1) var depth_differences: texture_2d<u32>;
@group(0) @binding(2) var ambient_occlusion: texture_storage_2d<r32float, write>;
@group(0) @binding(3) var<uniform> uniforms: Uniforms;

fn clamp_coordinates(pixel_coordinates: vec2<i32>) -> vec2<i32> {
    return clamp(pixel_coordinates, vec2<i32>(0i), vec2<i32>(uniforms.size) - 1i);
}

fn load_edges(pixel_coordinates: vec2<i32>) -> vec4<f32> {
    return unpack4x8unorm(textureLoad(depth_differences, clamp_coordinates(pixel_coordinates), 0i).r);
}

fn load_visibility(pixel_coordinates: vec2<i32>) -> f32 {
    return textureLoad(ambient_occlusion_noisy, clamp_coordinates(pixel_coordinates), 0i).r;
}

@compute
@workgroup_size(8, 8, 1)
fn spatial_denoise(@builtin(global_invocation_id) global_id: vec3<u32>) {
    let pixel_coordinates = vec2<i32>(global_id.xy);

    let left_edges = load_edges(pixel_coordinates + vec2<i32>(-1i, 0i));
    let right_edges = load_edges(pixel_coordinates + vec2<i32>(1i, 0i));
    let top_edges = load_edges(pixel_coordinates + vec2<i32>(0i, -1i));
    let bottom_edges = load_edges(pixel_coordinates + vec2<i32>(0i, 1i));
    var center_edges = load_edges(pixel_coordinates);
    center_edges *= vec4<f32>(left_edges.y, right_edges.x, top_edges.w, bottom_edges.z);

    let center_weight = 1.2;
    let left_weight = center_edges.x;
    let right_weight = center_edges.y;
    let top_weight = center_edges.z;
    let bottom_weight = center_edges.w;
    let top_left_weight = 0.425 * (top_weight * top_edges.x + left_weight * left_edges.z);
    let top_right_weight = 0.425 * (top_weight * top_edges.y + right_weight * right_edges.z);
    let bottom_left_weight = 0.425 * (bottom_weight * bottom_edges.x + left_weight * left_edges.w);
    let bottom_right_weight = 0.425 * (bottom_weight * bottom_edges.y + right_weight * right_edges.w);

    let center_visibility = load_visibility(pixel_coordinates);
    let left_visibility = load_visibility(pixel_coordinates + vec2<i32>(-1i, 0i));
    let right_visibility = load_visibility(pixel_coordinates + vec2<i32>(1i, 0i));
    let top_visibility = load_visibility(pixel_coordinates + vec2<i32>(0i, -1i));
    let bottom_visibility = load_visibility(pixel_coordinates + vec2<i32>(0i, 1i));
    let top_left_visibility = load_visibility(pixel_coordinates + vec2<i32>(-1i, -1i));
    let top_right_visibility = load_visibility(pixel_coordinates + vec2<i32>(1i, -1i));
    let bottom_left_visibility = load_visibility(pixel_coordinates + vec2<i32>(-1i, 1i));
    let bottom_right_visibility = load_visibility(pixel_coordinates + vec2<i32>(1i, 1i));

    var sum = center_visibility * center_weight;
    sum += left_visibility * left_weight;
    sum += right_visibility * right_weight;
    sum += top_visibility * top_weight;
    sum += bottom_visibility * bottom_weight;
    sum += top_left_visibility * top_left_weight;
    sum += top_right_visibility * top_right_weight;
    sum += bottom_left_visibility * bottom_left_weight;
    sum += bottom_right_visibility * bottom_right_weight;

    var sum_weight = center_weight;
    sum_weight += left_weight;
    sum_weight += right_weight;
    sum_weight += top_weight;
    sum_weight += bottom_weight;
    sum_weight += top_left_weight;
    sum_weight += top_right_weight;
    sum_weight += bottom_left_weight;
    sum_weight += bottom_right_weight;

    let denoised_visibility = sum / sum_weight;

    textureStore(ambient_occlusion, pixel_coordinates, vec4<f32>(denoised_visibility, 0.0, 0.0, 0.0));
}
)WGSL";

// Fullscreen multiply of the visibility over the frame (debug_view 1: the visibility alone).
constexpr const char* kCompositeShader = R"WGSL(
@group(0) @binding(0) var ambient_occlusion: texture_2d<f32>;
@group(0) @binding(1) var scene_depth: texture_2d<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> VertexOutput {
    var out: VertexOutput;
    let uv = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    out.position = vec4f(uv * vec2f(2.0, -2.0) + vec2f(-1.0, 1.0), 0.0, 1.0);
    out.uv = uv;
    return out;
}

fn sample_visibility(uv: vec2f) -> f32 {
    let coordinates = uv * uniforms.size - 0.5;
    let base = floor(coordinates);
    let fraction = coordinates - base;
    let max_coordinates = vec2i(uniforms.size) - 1i;
    let p00 = clamp(vec2i(base), vec2i(0i), max_coordinates);
    let p11 = clamp(vec2i(base) + 1i, vec2i(0i), max_coordinates);
    let v00 = textureLoad(ambient_occlusion, vec2i(p00.x, p00.y), 0i).r;
    let v10 = textureLoad(ambient_occlusion, vec2i(p11.x, p00.y), 0i).r;
    let v01 = textureLoad(ambient_occlusion, vec2i(p00.x, p11.y), 0i).r;
    let v11 = textureLoad(ambient_occlusion, vec2i(p11.x, p11.y), 0i).r;
    let top = mix(v00, v10, fraction.x);
    let bottom = mix(v01, v11, fraction.x);
    return mix(top, bottom, fraction.y);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let depth_size = vec2i(textureDimensions(scene_depth));
    let depth_pixel = clamp(vec2i(in.uv * vec2f(depth_size)), vec2i(0), depth_size - 1);
    if textureLoad(scene_depth, depth_pixel, 0).r < 0.0000001 {
        return vec4f(1.0);
    }
    let visibility = sample_visibility(in.uv);
    if uniforms.debug_view == 1u {
        return vec4f(visibility, visibility, visibility, 1.0);
    }
    let value = mix(1.0, visibility, uniforms.intensity);
    return vec4f(value, value, value, 1.0);
}
)WGSL";

// Mirror of the WGSL Uniforms struct.
struct AoUniforms {
    float projection[16];          // column-major
    float inverse_projection[16];  // column-major
    float size[2];
    float inv_size[2];
    float depth_scale[2];
    float effect_radius;
    float intensity;
    float slice_count;
    float samples_per_slice_side;
    uint32_t debug_view;
    float _pad;
};
static_assert(sizeof(AoUniforms) == 176);

// Raw handles: the views stay alive for the frame (Aurora's pooled depth snapshot) or longer
// (our targets, retired a few frames after a resize).
struct ComputePayload {
    WGPUTextureView depth;
    WGPUTextureView preprocessedDepthMips[5];
    WGPUTextureView preprocessedDepthAll;
    WGPUTextureView aoNoisy;
    WGPUTextureView depthDifferences;
    WGPUTextureView aoFinal;
    uint32_t uniformOffset;
    uint32_t uniformSize;
    uint32_t width;
    uint32_t height;
};
static_assert(sizeof(ComputePayload) <= gfx::InlineDrawPayloadSize);
static_assert(std::is_trivially_copyable_v<ComputePayload>);

struct CompositePayload {
    WGPUTextureView aoFinal;
    WGPUTextureView sceneDepth;
    uint32_t uniformOffset;
    uint32_t uniformSize;
};
static_assert(sizeof(CompositePayload) <= gfx::InlineDrawPayloadSize);
static_assert(std::is_trivially_copyable_v<CompositePayload>);

struct AoTargets {
    uint32_t width = 0;
    uint32_t height = 0;
    wgpu::Texture preprocessedDepth;
    wgpu::TextureView preprocessedDepthMips[5];
    wgpu::TextureView preprocessedDepthAll;
    wgpu::Texture aoNoisy;
    wgpu::TextureView aoNoisyView;
    wgpu::Texture depthDifferences;
    wgpu::TextureView depthDifferencesView;
    wgpu::Texture aoFinal;
    wgpu::TextureView aoFinalView;
};

struct RetiredTargets {
    AoTargets targets;
    int framesLeft = 0;
};

bool sInitTried = false;
bool sReady = false;
wgpu::Device sDevice;
wgpu::ComputePipeline sPreprocessPipeline, sMip4Pipeline, sGtaoPipeline, sDenoisePipeline;
wgpu::BindGroupLayout sPreprocessLayout, sMip4Layout, sGtaoLayout, sDenoiseLayout;
wgpu::Texture sHilbertLut;
wgpu::TextureView sHilbertLutView;
gfx::EncoderTaskId sComputeType = gfx::InvalidEncoderTask;
gfx::DrawTypeId sDrawType = gfx::InvalidDrawType;
AoTargets sTargets;
std::vector<RetiredTargets> sRetired;

// Composite pipeline, per EFB layout (render worker thread only).
wgpu::RenderPipeline sCompositePipeline;
wgpu::BindGroupLayout sCompositeLayout;
uint64_t sCompositeLayoutKey = 0;

wgpu::ShaderModule CreateShader(const char* label, const char* body) {
    std::string source = kUniforms;
    source += body;
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = source.c_str();
    const wgpu::ShaderModuleDescriptor desc{.nextInChain = &wgsl, .label = label};
    return sDevice.CreateShaderModule(&desc);
}

bool BuildComputePipeline(const char* label, const char* body, const char* entry, wgpu::ComputePipeline& outPipeline,
                          wgpu::BindGroupLayout& outLayout) {
    const wgpu::ShaderModule module = CreateShader(label, body);
    if (!module) {
        return false;
    }
    const wgpu::ComputePipelineDescriptor desc{
        .label = label,
        .compute = {.module = module, .entryPoint = entry},
    };
    outPipeline = sDevice.CreateComputePipeline(&desc);
    if (!outPipeline) {
        return false;
    }
    outLayout = outPipeline.GetBindGroupLayout(0);
    return static_cast<bool>(outLayout);
}

// Hilbert curve index LUT for the R2 noise sequence (Bevy's generate_hilbert_index_lut).
uint16_t HilbertIndex(uint16_t x, uint16_t y) {
    uint16_t index = 0;
    for (uint16_t level = 32; level > 0; level /= 2) {
        const uint16_t regionX = (x & level) > 0 ? 1 : 0;
        const uint16_t regionY = (y & level) > 0 ? 1 : 0;
        index += level * level * ((3 * regionX) ^ regionY);
        if (regionY == 0) {
            if (regionX == 1) {
                x = 63 - x;
                y = 63 - y;
            }
            std::swap(x, y);
        }
    }
    return index;
}

bool BuildHilbertLut() {
    const wgpu::TextureDescriptor desc{
        .label = "AO hilbert LUT",
        .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
        .size = {64, 64, 1},
        .format = wgpu::TextureFormat::R16Uint,
    };
    sHilbertLut = sDevice.CreateTexture(&desc);
    if (!sHilbertLut) {
        return false;
    }
    sHilbertLutView = sHilbertLut.CreateView();
    static uint16_t sLut[64 * 64];
    for (uint16_t y = 0; y < 64; ++y) {
        for (uint16_t x = 0; x < 64; ++x) {
            sLut[y * 64 + x] = HilbertIndex(x, y);
        }
    }
    const wgpu::TexelCopyTextureInfo dst{.texture = sHilbertLut};
    const wgpu::TexelCopyBufferLayout layout{.offset = 0, .bytesPerRow = 64 * 2, .rowsPerImage = 64};
    const wgpu::Extent3D extent{64, 64, 1};
    gfx::queue().WriteTexture(&dst, sLut, sizeof(sLut), &layout, &extent);
    return static_cast<bool>(sHilbertLutView);
}

bool EnsureTargets(uint32_t width, uint32_t height) {
    if (sTargets.width == width && sTargets.height == height) {
        return true;
    }
    if (sTargets.width != 0) {
        // Payloads with these views may still be in flight on the render worker.
        sRetired.push_back(RetiredTargets{std::exchange(sTargets, AoTargets{}), 4});
    }
    auto create = [&](const char* label, wgpu::TextureFormat format, uint32_t mips) {
        const wgpu::TextureDescriptor desc{
            .label = label,
            .usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding,
            .size = {width, height, 1},
            .format = format,
            .mipLevelCount = mips,
        };
        return sDevice.CreateTexture(&desc);
    };
    AoTargets t;
    t.preprocessedDepth = create("AO preprocessed depth", wgpu::TextureFormat::R32Float, 5);
    t.aoNoisy = create("AO noisy", wgpu::TextureFormat::R32Float, 1);
    t.depthDifferences = create("AO depth differences", wgpu::TextureFormat::R32Uint, 1);
    t.aoFinal = create("AO final", wgpu::TextureFormat::R32Float, 1);
    if (!t.preprocessedDepth || !t.aoNoisy || !t.depthDifferences || !t.aoFinal) {
        return false;
    }
    for (uint32_t mip = 0; mip < 5; ++mip) {
        const wgpu::TextureViewDescriptor viewDesc{.baseMipLevel = mip, .mipLevelCount = 1};
        t.preprocessedDepthMips[mip] = t.preprocessedDepth.CreateView(&viewDesc);
    }
    t.preprocessedDepthAll = t.preprocessedDepth.CreateView();
    t.aoNoisyView = t.aoNoisy.CreateView();
    t.depthDifferencesView = t.depthDifferences.CreateView();
    t.aoFinalView = t.aoFinal.CreateView();
    t.width = width;
    t.height = height;
    sTargets = std::move(t);
    return true;
}

constexpr uint32_t DivCeil(uint32_t n, uint32_t d) { return (n + d - 1) / d; }

// Render worker: the AO chain as one compute pass with four dispatches.
void OnCompute(const gfx::EncoderTaskContext& ctx, const wgpu::CommandEncoder& cmd, const void* payload,
               size_t payloadSize, void*) {
    if (payloadSize != sizeof(ComputePayload)) {
        return;
    }
    ComputePayload data;
    std::memcpy(&data, payload, sizeof(data));
    if (data.depth == nullptr) {
        return;
    }
    // C API for the entries: the payload holds borrowed raw handles.
    auto makeGroup = [&](const wgpu::BindGroupLayout& layout, std::initializer_list<std::pair<uint32_t, WGPUTextureView>> textures,
                         int uniformBinding) {
        std::vector<WGPUBindGroupEntry> entries;
        for (const auto& [binding, view] : textures) {
            WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
            e.binding = binding;
            e.textureView = view;
            entries.push_back(e);
        }
        if (uniformBinding >= 0) {
            WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
            e.binding = static_cast<uint32_t>(uniformBinding);
            e.buffer = ctx.uniformBuffer.Get();
            e.offset = data.uniformOffset;
            e.size = data.uniformSize;
            entries.push_back(e);
        }
        WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        desc.layout = layout.Get();
        desc.entryCount = entries.size();
        desc.entries = entries.data();
        return wgpu::BindGroup::Acquire(wgpuDeviceCreateBindGroup(ctx.device.Get(), &desc));
    };
    const wgpu::BindGroup preprocess = makeGroup(sPreprocessLayout,
        {{0, data.depth}, {1, data.preprocessedDepthMips[0]}, {2, data.preprocessedDepthMips[1]},
         {3, data.preprocessedDepthMips[2]}, {4, data.preprocessedDepthMips[3]}}, 5);
    const wgpu::BindGroup mip4 = makeGroup(sMip4Layout,
        {{6, data.preprocessedDepthMips[3]}, {7, data.preprocessedDepthMips[4]}}, -1);
    const wgpu::BindGroup gtao = makeGroup(sGtaoLayout,
        {{0, data.preprocessedDepthAll}, {1, sHilbertLutView.Get()}, {2, data.aoNoisy}, {3, data.depthDifferences}}, 4);
    const wgpu::BindGroup denoise = makeGroup(sDenoiseLayout,
        {{0, data.aoNoisy}, {1, data.depthDifferences}, {2, data.aoFinal}}, 3);
    if (!preprocess || !mip4 || !gtao || !denoise) {
        return;
    }
    const wgpu::ComputePassDescriptor passDesc{.label = "AO chain"};
    const wgpu::ComputePassEncoder pass = cmd.BeginComputePass(&passDesc);
    // Each preprocess workgroup covers 16x16 MIP-0 texels (8x8 invocations, 2x2 texels each).
    pass.SetPipeline(sPreprocessPipeline);
    pass.SetBindGroup(0, preprocess);
    pass.DispatchWorkgroups(DivCeil(data.width, 16), DivCeil(data.height, 16), 1);
    pass.SetPipeline(sMip4Pipeline);
    pass.SetBindGroup(0, mip4);
    pass.DispatchWorkgroups(DivCeil(std::max(data.width >> 4, 1u), 8), DivCeil(std::max(data.height >> 4, 1u), 8), 1);
    pass.SetPipeline(sGtaoPipeline);
    pass.SetBindGroup(0, gtao);
    pass.DispatchWorkgroups(DivCeil(data.width, 8), DivCeil(data.height, 8), 1);
    pass.SetPipeline(sDenoisePipeline);
    pass.SetBindGroup(0, denoise);
    pass.DispatchWorkgroups(DivCeil(data.width, 8), DivCeil(data.height, 8), 1);
    pass.End();
}

bool EnsureCompositePipeline(const gfx::RenderTargetLayout& layout) {
    if (sCompositePipeline && sCompositeLayoutKey == layout.key) {
        return true;
    }
    sCompositePipeline = nullptr;
    sCompositeLayout = nullptr;
    const wgpu::ShaderModule module = CreateShader("AO composite", kCompositeShader);
    if (!module) {
        return false;
    }
    // Multiply blend on the scene color; the alpha and the other attachments are left alone.
    const wgpu::BlendState blend{
        .color = {.operation = wgpu::BlendOperation::Add, .srcFactor = wgpu::BlendFactor::Dst,
                  .dstFactor = wgpu::BlendFactor::Zero},
        .alpha = {.operation = wgpu::BlendOperation::Add, .srcFactor = wgpu::BlendFactor::Zero,
                  .dstFactor = wgpu::BlendFactor::One},
    };
    wgpu::ColorTargetState targets[gfx::MaxColorAttachments];
    for (uint32_t i = 0; i < layout.colorAttachmentCount; ++i) {
        targets[i].format = layout.colorAttachments[i].format;
        if (layout.colorAttachments[i].semantic == gfx::ColorAttachmentSemantic::SceneColor) {
            targets[i].blend = &blend;
            targets[i].writeMask = wgpu::ColorWriteMask::All;
        } else {
            targets[i].writeMask = wgpu::ColorWriteMask::None;
        }
    }
    const wgpu::FragmentState fragment{
        .module = module,
        .entryPoint = "fs_main",
        .targetCount = layout.colorAttachmentCount,
        .targets = targets,
    };
    // The depth state must match the EFB pass even though depth is untouched.
    const wgpu::DepthStencilState depthStencil{
        .format = layout.depthStencilFormat,
        .depthWriteEnabled = wgpu::OptionalBool::False,
        .depthCompare = wgpu::CompareFunction::Always,
    };
    const wgpu::RenderPipelineDescriptor desc{
        .label = "AO composite",
        .vertex = {.module = module, .entryPoint = "vs_main"},
        .primitive = {.topology = wgpu::PrimitiveTopology::TriangleList},
        .depthStencil = layout.depthStencilFormat != wgpu::TextureFormat::Undefined ? &depthStencil : nullptr,
        .multisample = {.count = layout.sampleCount},
        .fragment = &fragment,
    };
    sCompositePipeline = sDevice.CreateRenderPipeline(&desc);
    if (!sCompositePipeline) {
        return false;
    }
    sCompositeLayout = sCompositePipeline.GetBindGroupLayout(0);
    sCompositeLayoutKey = layout.key;
    return static_cast<bool>(sCompositeLayout);
}

// Render worker: multiply the visibility over the EFB.
void OnDraw(const gfx::DrawContext& ctx, const wgpu::RenderPassEncoder& pass, const void* payload, size_t payloadSize,
            void*) {
    if (payloadSize != sizeof(CompositePayload) || !EnsureCompositePipeline(ctx.layout)) {
        return;
    }
    CompositePayload data;
    std::memcpy(&data, payload, sizeof(data));
    if (data.aoFinal == nullptr || data.sceneDepth == nullptr) {
        return;
    }
    WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = data.aoFinal;
    entries[1].binding = 1;
    entries[1].textureView = data.sceneDepth;
    entries[2].binding = 2;
    entries[2].buffer = ctx.uniformBuffer.Get();
    entries[2].offset = data.uniformOffset;
    entries[2].size = data.uniformSize;
    WGPUBindGroupDescriptor desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    desc.layout = sCompositeLayout.Get();
    desc.entryCount = 3;
    desc.entries = entries;
    const wgpu::BindGroup group = wgpu::BindGroup::Acquire(wgpuDeviceCreateBindGroup(ctx.device.Get(), &desc));
    if (!group) {
        return;
    }
    pass.SetPipeline(sCompositePipeline);
    pass.SetBindGroup(0, group);
    pass.Draw(3, 1, 0, 0);
}

bool Init() {
    sInitTried = true;
    sDevice = gfx::device();
    if (!sDevice) {
        return false;
    }
    if (!BuildComputePipeline("AO preprocess depth", kPreprocessShader, "preprocess_depth", sPreprocessPipeline,
                              sPreprocessLayout) ||
        !BuildComputePipeline("AO downsample mip4", kPreprocessShader, "downsample_mip4", sMip4Pipeline, sMip4Layout) ||
        !BuildComputePipeline("AO gtao", kGtaoShader, "gtao", sGtaoPipeline, sGtaoLayout) ||
        !BuildComputePipeline("AO denoise", kDenoiseShader, "spatial_denoise", sDenoisePipeline, sDenoiseLayout) ||
        !BuildHilbertLut()) {
        std::fprintf(stderr, "[pc] ambient occlusion: pipeline creation failed\n");
        return false;
    }
    sComputeType = gfx::register_encoder_task_type({.label = "AO chain", .callback = OnCompute});
    sDrawType = gfx::register_draw_type({.label = "AO composite", .draw = OnDraw});
    return sComputeType != gfx::InvalidEncoderTask && sDrawType != gfx::InvalidDrawType;
}

void Invert4x4(const float m[16], float out[16]) {
    // Column-major general inverse (cofactors).
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    det = det != 0.0f ? 1.0f / det : 0.0f;
    for (int i = 0; i < 16; i++) {
        out[i] = inv[i] * det;
    }
}

}  // namespace

// GameScene::draw3D, after the opaque draws.
void pc_ssao_draw() {
    for (auto it = sRetired.begin(); it != sRetired.end();) {
        it = --it->framesLeft <= 0 ? sRetired.erase(it) : it + 1;
    }
    // SMGPC_SSAO_DEBUG=1: show the visibility alone.
    static const bool sDebug = std::getenv("SMGPC_SSAO_DEBUG") != nullptr;
    const float strength = pc_settings_ssao();
    if (strength <= 0.0f || gfx::is_offscreen()) {
        return;
    }
    if (!sInitTried) {
        sReady = Init();
        std::fprintf(stderr, "[pc] ambient occlusion %s\n", sReady ? "ready" : "unavailable");
    }
    if (!sReady) {
        return;
    }

    gfx::ResolvedTargets resolved;
    if (!gfx::resolve_pass({.color = false, .depth = true, .normal = false}, resolved) || !resolved.depth) {
        return;
    }
    // The AO runs at the window's resolution at most (half the render size when supersampling).
    const uint32_t divisor = resolved.width > 2400 ? 2 : 1;
    const uint32_t width = resolved.width / divisor;
    const uint32_t height = resolved.height / divisor;
    if (width < 32 || height < 32 || !EnsureTargets(width, height)) {
        return;
    }

    // The camera's GX projection as Aurora sends it (reversed Z: row 2 negated), column-major.
    const TProj3f& gxProj = MR::getCameraProjectionMtx();
    AoUniforms u{};
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            float v = gxProj.mMtx[row][col];
            if (row == 2 && gfx::uses_reversed_z()) {
                v = -v;
            } else if (row == 2) {
                v += gxProj.mMtx[3][col];
            }
            u.projection[col * 4 + row] = v;
        }
    }
    Invert4x4(u.projection, u.inverse_projection);
    u.size[0] = static_cast<float>(width);
    u.size[1] = static_cast<float>(height);
    u.inv_size[0] = 1.0f / u.size[0];
    u.inv_size[1] = 1.0f / u.size[1];
    u.depth_scale[0] = static_cast<float>(resolved.width) / u.size[0];
    u.depth_scale[1] = static_cast<float>(resolved.height) / u.size[1];
    u.effect_radius = 90.0f;  // game units (Mario is about 160 tall)
    u.intensity = std::min(strength, 1.0f);
    u.slice_count = 3.0f;  // XeGTAO "high": 3 slices x 3 samples per side
    u.samples_per_slice_side = 3.0f;
    u.debug_view = sDebug ? 1 : 0;
    const gfx::Range uniforms = gfx::push_uniform(reinterpret_cast<const uint8_t*>(&u), sizeof(u));
    if (uniforms.size == 0) {
        return;
    }

    ComputePayload compute{};
    compute.depth = resolved.depth.Get();
    for (int mip = 0; mip < 5; ++mip) {
        compute.preprocessedDepthMips[mip] = sTargets.preprocessedDepthMips[mip].Get();
    }
    compute.preprocessedDepthAll = sTargets.preprocessedDepthAll.Get();
    compute.aoNoisy = sTargets.aoNoisyView.Get();
    compute.depthDifferences = sTargets.depthDifferencesView.Get();
    compute.aoFinal = sTargets.aoFinalView.Get();
    compute.uniformOffset = uniforms.offset;
    compute.uniformSize = uniforms.size;
    compute.width = width;
    compute.height = height;
    if (!gfx::push_encoder_task(sComputeType, &compute, sizeof(compute))) {
        return;
    }
    const CompositePayload composite{sTargets.aoFinalView.Get(), resolved.depth.Get(), uniforms.offset, uniforms.size};
    gfx::push_custom_draw(sDrawType, &composite, sizeof(composite));
}
