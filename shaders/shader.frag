#version 410 core

out vec4 frag_colour;
in vec2 uv;
in vec3 ray;

struct Material {
    vec3 diffuse_col;
    float albedo; // diffuse reflectance
    float fresnel_0; // fresnel when looking straight at surface.
};

struct Plane {
    vec3 normal;
    float offset; // Offset of plane in direction of normal
    Material mat;
};

struct Box {
    vec3 centre;
    vec3 dimensions;
    Material mat;
};

struct Sphere {
    vec3 centre;
    float radius;
    Material mat;
};

// Misc
uniform float time;
uniform vec2 resolution;

// Camera uniforms:
uniform vec3 cam_pos;
uniform vec3 view_dir;

// SDFs
const int MAX_BOXES = 16;
uniform int num_boxes;
uniform Box boxes[MAX_BOXES];

const int MAX_SPHERES = 16;
uniform int num_spheres;
uniform Sphere spheres[MAX_SPHERES];

const int MAX_PLANES = 16;
uniform int num_planes;
uniform Plane planes[MAX_PLANES];

int total_primitives = num_boxes + num_spheres + num_planes;
// Raymarch settings
const int MAX_STEPS = 1000;
const float EPS = 0.001;
const float NEAR_SPHERE = 0.1;

float far_field = 1000.0;

// Attenuation constants
const float constant = 1.0;
const float linear = 0.09;
const float quadratic = 0.032;

// Lights 
vec3 dir_light = normalize(-vec3(0.5, 1.0, 0.5));

struct PointLight {
    vec3 pos;
    vec3 colour;
};

const int MAX_POINT_LIGHTS = 4;
int num_point_lights = 1;
PointLight point_lights[MAX_POINT_LIGHTS];

// Other settings
vec3 bg_colour = vec3(0.5, 0.8, 1.0);
float ambient_max_str = 0.3;
vec3 obj_colour = vec3(0.8, 0.6, 0.7);

// Mirror settings
const int MAX_BOUNCES = 50;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float SDF_sphere_periodic(vec3 p, Sphere sphere) {
    float period = sphere.radius * 10.0;

    vec3 q = mod(p - sphere.centre + 0.5 * period, period)
        - 0.5 * period;

    return length(q) - sphere.radius;
}

float SDF_sphere(vec3 p, Sphere sphere) {
    return length(p - sphere.centre) - sphere.radius;
}

float SDF_plane(vec3 p, Plane plane) {
    return abs(dot(p - plane.normal * plane.offset, plane.normal)); 
}

float SDF_box(vec3 p, Box box) {
    vec3 q = abs(p - box.centre) - box.dimensions;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

const Material dummy_mat = Material(vec3(0.0), 0.0, 0.0);

Material get_SDF_mat(vec3 p) {
    float sdf = 1e9;
    for (int i = 0; i < num_spheres; i++) {
        if (i == 5) {
            sdf = min(sdf, SDF_sphere_periodic(p, spheres[i])); 
        }
        else sdf = min(sdf, SDF_sphere(p, spheres[i])); 
        if (sdf < EPS) {
            return spheres[i].mat;
        }
    }
    for (int i = 0; i < num_planes; i++) {
        sdf = min(sdf, SDF_plane(p, planes[i]));
        if (sdf < EPS) {
            return planes[i].mat;
        }
    }
    for (int i = 0; i < num_boxes; i++) {
        sdf = min(sdf, SDF_box(p, boxes[i]));
        if (sdf < EPS) {
            return boxes[i].mat;
        }
    }
    return dummy_mat;
}

float SDF(vec3 p) {
    float sdf = 1e9;
    for (int i = 0; i < num_spheres; i++) {
        if (i == 5) {
            sdf = min(sdf, SDF_sphere_periodic(p, spheres[i])); 
        }
        else sdf = min(sdf, SDF_sphere(p, spheres[i])); 
    }
    for (int i = 0; i < num_planes; i++) {
        sdf = min(sdf, SDF_plane(p, planes[i]));
    }
    for (int i = 0; i < num_boxes; i++) {
        sdf = min(sdf, SDF_box(p, boxes[i]));
    }
    return sdf;
}

vec3 calc_normal(vec3 p) {
    vec2 sample_eps = vec2(0.002, 0.0);
    return normalize(vec3(
        SDF(p + sample_eps.xyy) - SDF(p - sample_eps.xyy),
        SDF(p + sample_eps.yxy) - SDF(p - sample_eps.yxy),
        SDF(p + sample_eps.yyx) - SDF(p - sample_eps.yyx)
    ));
}

struct MarchResult {
    bool has_hit;
    vec3 hit_pos;
    float dist_marched;
    Material mat;

    int num_calls;
};

MarchResult ray_march(vec3 start, vec3 ray_d) {
    int num_calls = 0;

    float t = 0.0;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (t > far_field) {
            return MarchResult(false, vec3(0), 1e9, dummy_mat, num_calls);
        }

        vec3 cur_pos = start + ray_d * t;

        float sdf = SDF(cur_pos);
        num_calls += total_primitives;

        if (sdf < EPS) {
            num_calls += total_primitives;
            return MarchResult(true, cur_pos, t, get_SDF_mat(cur_pos), num_calls);
        }

        t += sdf;
    }

    return MarchResult(false, vec3(0), 1e9, dummy_mat, num_calls);
}

float soft_shadows(vec3 start, vec3 ray_d, float light_dist) {
    float t = 0.0;
    float light_str = 1.0;
    const float shadow_const = 16.0;

    for (int i = 0; i < MAX_STEPS; i++) {
        if (t >= light_dist) {
            break;
        }

        vec3 cur_pos = start + ray_d * t;
        float sdf = SDF(cur_pos);

        light_str = min(light_str, shadow_const * sdf / t);

        if (sdf < EPS) {
            return 0.0;
        }

        t += sdf;
    }

    return clamp(light_str, 0.0, 1.0); 
}

float AO(vec3 start, vec3 normal) {
    float ao = 0.0;
    float weight = 1.0;
    int ao_steps = 5;

    for (int i = 0; i < ao_steps; i++) {
        float step_size = 0.01 + i * 0.03;
        vec3 ray_pos = start + normal * step_size;
        float sdf = SDF(ray_pos);
        
        ao += (step_size - sdf) * weight;

        weight *= 0.5;
    }

    return clamp(1.0 - 10.0 * ao, 0.0, 1.0);
}   

void main() {
    // init point lights
    point_lights[0] = PointLight(vec3(0, 2, 0), vec3(0.2, 0.5, 0.8));
    point_lights[1] = PointLight(vec3(2, 2, 2), vec3(0.2, 0.5, 0.8));

    // Move up and down
    //point_lights[0].pos.y += 1 * sin(time);

    vec3 colour = vec3(0.0);
    float reflec_strength = 1.0;

    vec3 ray_dir = normalize(ray);
    vec3 start_pos = cam_pos + ray * NEAR_SPHERE;

    // For debug. x = for normals, y = for normal marches, z = for shadow marches
    vec3 num_sdf_calls = vec3(0);

    // Raymarching Loop:
    for (int bounces = 0; bounces < MAX_BOUNCES; ++bounces) {
        MarchResult march_res = ray_march(start_pos, ray_dir);
        num_sdf_calls.y += int(march_res.num_calls);
        vec3 contrib = vec3(0.0);

        if (!march_res.has_hit) {
            contrib += bg_colour;
            contrib *= reflec_strength;
            colour += contrib;
            break;
        }
        else {
            // cache some values
            float albedo = march_res.mat.albedo;
            vec3 diffuse_colour = march_res.mat.diffuse_col;
            float fresnel_0 = march_res.mat.fresnel_0;

            vec3 cur_pos = march_res.hit_pos;
            vec3 normal = calc_normal(cur_pos);
            num_sdf_calls.x += 6.0;

            // slighly offset normal for cosmetic effect.
            // float randx = random(vec2(ray.x, ray.y) * time);
            // float randy = random(vec2(ray.y, ray.z) * time);
            // float randz = random(vec2(ray.z, ray.x) * time);
            // vec3 rand_dir = vec3(randx, randy, randz) / 50.0;

            // vec3 cosmetic_normal = normalize(normal + rand_dir);

            // cache some values 
            float cos_theta = max(dot(-ray_dir, normal), 0.0);
            float fresnel = fresnel_0 + (1-fresnel_0) * pow((1-cos_theta), 5.0);
        
            contrib += ambient_max_str * obj_colour * AO(cur_pos, normal);
            num_sdf_calls.z += 5.0;

            vec3 reflected_view = reflect(ray_dir, normal);

            for (int i = 0; i < num_point_lights; i++) {
                // Diffuse
                vec3 light_vec = point_lights[i].pos - cur_pos;
                float dist = length(light_vec);
                vec3 light_dir = light_vec / dist;
                float attenuation = 1.0 / (constant + linear * dist + quadratic * (dist * dist));
                float diffuse_str = clamp(dot(normal, light_dir), 0.0, 1.0) * attenuation * albedo;

                float check_occ = soft_shadows(cur_pos + 2 * EPS * normal, light_dir, dist);

                //num_sdf_calls.z += int(check_occ.num_calls);

                if (check_occ == 0.0) {
                    continue;
                }
                
                contrib += diffuse_str * point_lights[i].colour * diffuse_colour * check_occ;
                // Specular
                float specular_str = pow(max(0.0, dot(light_dir, reflected_view)), 16.0) * attenuation;
                contrib += vec3(0.2) * specular_str * fresnel * check_occ;
            }

            // Sunlight
            float diffuse_str = clamp(-dot(normal, dir_light), 0.0, 1.0) * albedo;
            float check_occ = soft_shadows(cur_pos + 2 * EPS * normal, -dir_light, far_field);

            // num_sdf_calls.z += int(check_occ.num_calls);

            if (check_occ != 0.0) {
                contrib += diffuse_str * bg_colour * diffuse_colour * check_occ;
                // Specular
                float specular_str = pow(max(0.0, dot(-dir_light, reflected_view)), 16.0);
                contrib += vec3(0.2) * specular_str * fresnel * check_occ;
            }

            // Reduce contribution based on how many bounces already (reflect strength)
            contrib *= reflec_strength;
            colour += contrib;

            // repeat everything again but with new ray direction and reduction in strength
            if (fresnel_0 != 0.0) {
                reflec_strength *= fresnel;
                ray_dir = reflect(ray_dir, normal);
                start_pos = cur_pos + 2 * EPS * normal; // move along normal a bit.
            }  
            else {
                break;
            }
        }
        num_sdf_calls = vec3(0.0);
    }

    frag_colour = vec4(num_sdf_calls.y / 1024.0, num_sdf_calls.y / 1024.0, num_sdf_calls.y / 1024.0, 1.0);
}