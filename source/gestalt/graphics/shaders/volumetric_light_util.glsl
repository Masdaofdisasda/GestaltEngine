#define FROXEL_DISPATCH_X 8
#define FROXEL_DISPATCH_Y 8
#define FROXEL_DISPATCH_Z 1

#define PI 3.1415926538

vec3 world_position_from_depth( vec2 uv, float raw_depth, mat4 inverse_view_projection ) {

    vec4 H = vec4( uv.x * 2 - 1, (1 - uv.y) * 2 - 1, raw_depth, 1.0 );
    vec4 D = inverse_view_projection * H;

    return D.xyz / D.w;
}

vec3 view_position_from_depth( vec2 uv, float raw_depth, mat4 inverse_projection ) {

    vec4 H = vec4( uv.x * 2 - 1, (1 - uv.y) * 2 - 1, raw_depth, 1.0 );
    vec4 D = inverse_projection * H;

    return D.xyz / D.w;
}

vec2 uv_from_pixels( ivec2 pixel_position, uint width, uint height ) {
    return pixel_position / vec2(width * 1.f, height * 1.f);
}
// Convert raw_depth (0..1) to linear depth (near...far)
float raw_depth_to_linear_depth( float raw_depth, float near, float far ) {
    return near * far / (far + raw_depth * (near - far));
}

// Convert linear depth (near...far) to raw_depth (0..1)
float linear_depth_to_raw_depth( float linear_depth, float near, float far ) {
    return ( near * far ) / ( linear_depth * ( near - far ) ) - far / ( near - far );
}

// Exponential distribution as in https://advances.realtimerendering.com/s2016/Siggraph2016_idTech6.pdf
// Convert slice index to (near...far) value distributed with exponential function.
float slice_to_exponential_depth( float near, float far, int slice, int num_slices ) {
    return near * pow( far / near, (float(slice) + 0.5f) / float(num_slices) );
}

float slice_to_exponential_depth_jittered( float near, float far, float jitter, int slice, int num_slices ) {
    return near * pow( far / near, (float(slice) + 0.5f + jitter) / float(num_slices) );
}

// http://www.aortiz.me/2018/12/21/CG.html
// Convert linear depth (near...far) to (0...1) value distributed with exponential functions
// like slice_to_exponential_depth.
// This function is performing all calculations, a more optimized one precalculates factors on CPU.
float linear_depth_to_uv( float near, float far, float linear_depth, int num_slices ) {
    const float one_over_log_f_over_n = 1.0f / log2( far / near );
    const float scale = num_slices * one_over_log_f_over_n;
    const float bias = - ( num_slices * log2(near) * one_over_log_f_over_n );

    return max(log2(linear_depth) * scale + bias, 0.0f) / float(num_slices);
}

// Convert linear depth (near...far) to (0...1) value distributed with exponential functions.
// Uses CPU cached values of scale and bias.
float linear_depth_to_uv_optimized( float scale, float bias, float linear_depth, int num_slices ) {
    return max(log2(linear_depth) * scale + bias, 0.0f) / float(num_slices);
}

// Noise helper functions
float remap_noise_tri( float v ) {
    v = v * 2.0 - 1.0;
    return sign(v) * (1.0 - sqrt(1.0 - abs(v)));
}

// Takes 2 noises in space [0..1] and remaps them in [-1..1]
float triangular_noise( float noise0, float noise1 ) {
    return noise0 + noise1 - 1.0f;
}

float interleaved_gradient_noise(vec2 pixel, int frame) {
    pixel += (float(frame) * 5.588238f);
    return fract(52.9829189f * fract(0.06711056f*float(pixel.x) + 0.00583715f*float(pixel.y)));  
}

float ToLinear1 (float c) {
    return ( c <= 0.04045 ) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float generate_noise(vec2 pixel, int frame, float scale, uint noise_type, uvec3 froxel_dimensions, sampler2D blue_noise) {
    // Animated blue noise using golden ratio.
    if (noise_type == 0) {
        vec2 uv = vec2(pixel.xy / froxel_dimensions.xy);
        // Read blue noise from texture
        vec2 blue_noise = texture(blue_noise, uv ).rg;
        const float k_golden_ratio_conjugate = 0.61803398875;
        float blue_noise0 = fract(ToLinear1(blue_noise.r) + float(frame % 256) * k_golden_ratio_conjugate);
        float blue_noise1 = fract(ToLinear1(blue_noise.g) + float(frame % 256) * k_golden_ratio_conjugate);

        return triangular_noise(blue_noise0, blue_noise1) * scale;
    }
    // Interleaved gradient noise
    if (noise_type == 1) {
        float noise0 = interleaved_gradient_noise(pixel, frame);
        float noise1 = interleaved_gradient_noise(pixel, frame + 1);

        return triangular_noise(noise0, noise1) * scale;
    }

    // Initial noise attempt, left for reference.
    return (interleaved_gradient_noise(pixel, frame) * scale) - (scale * 0.5f);
}

// Coordinate transformations
vec2 uv_from_froxels( vec2 froxel_position, uint width, uint height ) {
    return froxel_position / vec2(width * 1.f, height * 1.f);
}

vec3 world_from_froxel(ivec3 froxel_coord, vec2 halton_xy, float temporal_reprojection_jitter_scale, uvec3 froxel_dimensions, int current_frame, float noise_scale, uint noise_type, sampler2D blue_noise, float froxel_near, float froxel_far, mat4 froxel_inverse_view_projection) {

    vec2 uv = uv_from_froxels( froxel_coord.xy + vec2(0.5) + halton_xy * temporal_reprojection_jitter_scale, froxel_dimensions.x, froxel_dimensions.y );

    // First calculate linear depth
    float depth = froxel_coord.z * 1.0f / float(froxel_dimensions.z);

    // Exponential depth
    float jittering = generate_noise(froxel_coord.xy * 1.0f, current_frame, noise_scale, noise_type, froxel_dimensions, blue_noise);
    float linear_depth = slice_to_exponential_depth_jittered( froxel_near, froxel_far, jittering, froxel_coord.z, int(froxel_dimensions.z) );

    // Then calculate raw depth, necessary to properly calculate world position.
    float raw_depth = linear_depth_to_raw_depth(linear_depth, froxel_near, froxel_far);
    return world_position_from_depth( uv, raw_depth, froxel_inverse_view_projection );
}

vec3 world_from_froxel_no_jitter(ivec3 froxel_coord, uvec3 froxel_dimensions, float froxel_near, float froxel_far, mat4 froxel_inverse_view_projection) {

    vec2 uv = uv_from_froxels( froxel_coord.xy + vec2(0.5), froxel_dimensions.x, froxel_dimensions.y );

    // First calculate linear depth
    float depth = froxel_coord.z * 1.0f / float(froxel_dimensions.z);

    // Exponential depth
    float linear_depth = slice_to_exponential_depth( froxel_near, froxel_far, froxel_coord.z, int(froxel_dimensions.z) );
    // Then calculate raw depth, necessary to properly calculate world position.
    float raw_depth = linear_depth_to_raw_depth(linear_depth, froxel_near, froxel_far);
    return world_position_from_depth( uv, raw_depth, froxel_inverse_view_projection );
}

// Phase functions ///////////////////////////////////////////////////////
// Equations from http://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf
float henyey_greenstein(float g, float costh) {
    const float numerator = 1.0 - g * g;
    const float denominator = 4.0 * PI * pow(1.0 + g * g - 2.0 * g * costh, 3.0/2.0);
    return numerator / denominator;
}

float shlick(float g, float costh) {
    const float numerator = 1.0 - g * g;
    const float g_costh = g * costh;
    const float denominator = 4.0 * PI * ((1 + g_costh) * (1 + g_costh));
    return numerator / denominator;
}

float cornette_shanks(float g, float costh) {
    const float numerator = 3.0 * (1.0 - g * g) * (1.0 + costh * costh);
    const float denominator = 4.0 * PI * 2.0 * (2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * costh, 3.0/2.0);
    return numerator / denominator;
}

// Evaluates the Draine phase function
// u = dot(prev_dir, next_dir)
// g = HG asymmetry parameter (-1 <= g <= 1)
// a = "alpha" shape parameter (usually related to g)

float evalDraine(float u, float g, float a)
{
    float denom = pow(1.0 + g * g - 2.0 * g * u, 1.5);
    float numerator = (1.0 - g * g) * (1.0 + a * u * u);
    float normalization = 4.0 * (1.0 + (a * (1.0 + 2.0 * g * g)) / 3.0) * PI;
    return numerator / (normalization * denom);
}

void computePhaseFunctionParameters(float d, out float g_hg, out float g_d, out float alpha, out float w_d)
{
    if (d <= 0.1)
    {
        // Small particles (d <= 0.1 µm)
        g_hg = 13.8 * d * d;
        g_d = 1.1456 * d * sin(9.29044 * d);
        alpha = 250.0;
        w_d = 0.252977 - 312.983 * pow(d, 4.3);
    }
    else if (d <= 1.5)
    {
        // Mid-range particles (0.1 µm < d < 1.5 µm)
        float log_d = log2(d);
        float log_d_ln = log(d);

        g_hg = 0.862 - 0.143 * log_d;

        float numerator = 1.19692 * cos((log_d_ln - 0.238604) / (pow(log_d_ln + 1.00667, 0.507522) - 0.15677 * log_d_ln));
        float denominator = 3.38707 * log_d_ln + 2.11193;
        g_d = 0.379685 * cos(numerator + 1.37932 * log_d_ln + 0.0625835) + 0.344213;

        alpha = 250.0;

        w_d = 0.146209 * cos(3.38707 * log_d_ln + 2.11193) + 0.316072 + 0.0778917 * log_d_ln;
    }
    else if (d < 5.0)
    {
        // Mid-range particles (1.5 µm <= d < 5 µm)
        float log_d = log(d);
        float log_log_d = log(log_d);

        g_hg = 0.0604931 * log(log_d) + 0.940256;

        g_d = 0.500411 - 0.081287 / (-2.0 * log_d + tan(log_d) + 1.27551);

        alpha = 7.30354 * log_d + 6.31675;

        w_d = 0.026914 * (log_d - cos(5.68947 * (log_log_d - 0.0292149))) + 0.376475;
    }
    else if (d <= 50.0)
    {
        // Large particles (5 µm <= d <= 50 µm)
        g_hg = exp(-0.0990567 / (d - 1.67154));

        g_d = exp(-2.20679 / (d + 3.91029)) - 0.428934;

        alpha = exp(3.62489 - 8.29288 / (d + 5.52825));

        w_d = exp(-0.599085 / (d - 0.641583)) - 0.665888;
    }
    else
    {
        // Handle out-of-range values (you can adjust this as needed)
        g_hg = 0.0;
        g_d = 0.0;
        alpha = 0.0;
        w_d = 0.0;
    }
}


// Choose different phase functions
float phase_function(vec3 V, vec3 L, float g, uint phase_function_type) {
    float cos_theta = dot(V, L);

    if (phase_function_type == 0) {
        return henyey_greenstein(g, cos_theta);
    }
    if (phase_function_type == 1) {
        return cornette_shanks(g, cos_theta);
    }
    if (phase_function_type == 2) {
        return shlick(g, cos_theta);
    }

    // https://research.nvidia.com/labs/rtr/approximate-mie/
    float d = g; // 0.1 < d < 50 in µm

    float g_hg = 0.0;
    float g_d = 0.0;
    float alpha =  0.0;
    float w_d = 0.0;

    computePhaseFunctionParameters(d, g_hg, g_d, alpha, w_d);

    float HG = henyey_greenstein(g_hg, cos_theta);
    float D = evalDraine(cos_theta, g_d, alpha);

    return  (1.0 - w_d) * HG + w_d * D;
}

float remap( float value, float oldMin, float oldMax, float newMin,float newMax) {
    return newMin + (value - oldMin) / (oldMax - oldMin) * (newMax - newMin);
}

vec4 scattering_extinction_from_color_density( vec3 color, float density, float scattering_factor ) {

    const float extinction = scattering_factor * density;
    return vec4( color * extinction, extinction );
}

vec3 apply_volumetric_fog( vec2 screen_uv, float raw_depth, vec3 color, float near, float far, sampler3D fog_texture ) {

    // Fog linear depth distribution
    float linear_depth = raw_depth_to_linear_depth( raw_depth, near, far );
    //float depth_uv = linear_depth / far;
    // Exponential
    const int volumetric_fog_num_slices = 128;
    float depth_uv = linear_depth_to_uv( near, far, linear_depth, volumetric_fog_num_slices );
    vec3 froxel_uvw = vec3(screen_uv.xy, depth_uv);
    vec4 scattering_transmittance = texture(fog_texture, froxel_uvw);

    //const float scattering_modifier = enable_volumetric_fog_opacity_anti_aliasing() ? max( 1 - scattering_transmittance.a, 0.00000001f ) : 1.0f;
    const float scattering_modifier = 1.0f;

    color.rgb = color.rgb * scattering_transmittance.a + scattering_transmittance.rgb * scattering_modifier;

    return color;
}