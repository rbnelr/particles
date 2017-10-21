
#define _USING_V110_SDK71_ 1
#include "windows.h"

#include "lang_helpers.hpp"
#include "math.hpp"
#include "vector/vector.hpp"

typedef s32v2	iv2;
typedef s32v3	iv3;
typedef s32v4	iv4;
typedef fv2		v2;
typedef fv3		v3;
typedef fv4		v4;
typedef fm2		m2;
typedef fm3		m3;
typedef fm4		m4;

#include <cstdio>

namespace random {
	static void init_same_seed_everytime () {
		srand(0);
	}
	static f32 f32_01 () {
		return (f32)rand() / (f32)RAND_MAX;
	}
	static v3 v3_01 () {
		return (v3)(iv3(rand(), rand(), rand())) / v3((f32)RAND_MAX);
	}
	static v2 v2_n1p1 () {
		return ((v2)(iv2(rand(), rand())) / v2((f32)RAND_MAX)) * v2(2) -v2(1);
	}
	
}

#include "glad.c"
#include "GLFW/glfw3.h"

STATIC_ASSERT(sizeof(GLint) ==		sizeof(s32));
STATIC_ASSERT(sizeof(GLuint) ==		sizeof(u32));
STATIC_ASSERT(sizeof(GLsizei) ==	sizeof(u32));
STATIC_ASSERT(sizeof(GLsizeiptr) ==	sizeof(u64));

static GLFWwindow*	wnd;
static u32			frame_indx; // probably should only used for debug logic

static iv2			wnd_dim;
static v2			wnd_dim_aspect;
static iv2			cursor_pos;
static s32			scrollwheel_diff;

#include "buttons.hpp"

static void glfw_error_proc (int err, const char* msg) {
	fprintf(stderr, ANSI_COLOUR_CODE_RED "GLFW Error! 0x%x '%s'\n" ANSI_COLOUR_CODE_NC, err, msg);
}
static void glfw_scroll_proc (GLFWwindow* window, double xoffset, double yoffset) {
	scrollwheel_diff += (s32)floor(yoffset);
}

static void setup_glfw () {
	glfwSetErrorCallback(glfw_error_proc);
	
	dbg_assert( glfwInit() );
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,	3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,	1);
	//glfwWindowHint(GLFW_OPENGL_PROFILE,			GLFW_OPENGL_CORE_PROFILE);
	
	glfwWindowHint(GLFW_VISIBLE,				0);
	wnd = glfwCreateWindow(1000, 1000, "GLFW test", NULL, NULL);
	dbg_assert(wnd);
	
	glfwSetKeyCallback(wnd, glfw_key_proc);
	glfwSetScrollCallback(wnd, glfw_scroll_proc);
	glfwSetMouseButtonCallback(wnd, glfw_mousebutton_proc);
	
	glfwMakeContextCurrent(wnd);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
}

//
struct Camera {
	v2	pos_world;
	f32	radius; // zoom adjusted so that either window width or height fits 2*radus (whichever is smaller)
	
	m4 world_to_clip;
};
static Camera					cam;

static GLuint vbo_init_and_upload_static (void* data, uptr data_size) {
	GLuint vbo;
	glGenBuffers(1, &vbo);
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data_size, data, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	
	return vbo;
}
static bool shad_check_compile_status (GLuint shad) {
	GLint status;
	glGetShaderiv(shad, GL_COMPILE_STATUS, &status);
	bool error = status == GL_FALSE;
	
	s32 log_size; // including null terminator
	glGetShaderiv(shad, GL_INFO_LOG_LENGTH, &log_size);
	
	if (log_size > 1) { // 0=>no log  1=>empty log
		char* log_str = (char*)malloc(log_size);
		defer { free(log_str); };
		
		s32 written_len;
		glGetShaderInfoLog(shad, log_size, &written_len, log_str);
		dbg_assert(written_len == (log_size -1) && log_str[written_len] == '\0');
		
		if (error) {
			printf(	ANSI_COLOUR_CODE_RED
					"Shader compilation failed!\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		} else {
			printf(	ANSI_COLOUR_CODE_YELLOW
					"Shader compilation info log:\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		}
	}
	return !error;
}
static bool shad_check_link_status (GLuint prog) {
	GLint status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	bool error = status == GL_FALSE;
	
	s32 log_size; // including null terminator
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_size);
	
	if (log_size > 1) { // 0=>no log  1=>empty log
		char* log_str = (char*)malloc(log_size);
		defer { free(log_str); };
		
		s32 written_len;
		glGetProgramInfoLog(prog, log_size, &written_len, log_str);
		dbg_assert(written_len == (log_size -1) && log_str[written_len] == '\0');
		
		if (error) {
			printf(	ANSI_COLOUR_CODE_RED
					"Shader linking failed!\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		} else {
			printf(	ANSI_COLOUR_CODE_YELLOW
					"Shader linking info log:\n"
					"  '%s'\n" ANSI_COLOUR_CODE_NC, log_str);
		}
	}
	return !error;
}

struct Basic_Shader {
	GLuint		prog;
	
	cstr		vert_src;
	cstr		frag_src;
	
	Basic_Shader (cstr v, cstr f): vert_src{v}, frag_src{f} {}
	
	void compile () {
		prog = glCreateProgram();
		
		GLuint vert = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert, 1, &vert_src, NULL);
		glCompileShader(vert);
		shad_check_compile_status(vert);
		glAttachShader(prog, vert);
		
		GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag, 1, &frag_src, NULL);
		glCompileShader(frag);
		shad_check_compile_status(frag);
		glAttachShader(prog, frag);
		
		glLinkProgram(prog);
		shad_check_link_status(prog);
		
		glDetachShader(prog, vert);
		glDeleteShader(vert);
		
		glDetachShader(prog, frag);
		glDeleteShader(frag);
	}
	void bind () {
		glUseProgram(prog);
	}
	
};
struct Unif_fm2 {
	GLint loc;
	void set (fm2 m) const {
		glUniformMatrix2fv(loc, 1, GL_FALSE, &m.arr[0][0]);
	}
};
struct Unif_fm4 {
	GLint loc;
	void set (fm4 m) const {
		glUniformMatrix4fv(loc, 1, GL_FALSE, &m.arr[0][0]);
	}
};
struct Unif_fv2 {
	GLint loc;
	void set (fv2 v) const {
		glUniform2f(loc, v.x,v.y);
	}
};
struct Unif_fv3 {
	GLint loc;
	void set (fv3 cr v) const {
		glUniform3fv(loc, 1, &v.x);
	}
};

#if 0
struct VBO_Particle {
	GLuint	vbo;
	struct V {
		v2	pos_clip; // calculate everything on cpu for now, could be optimized to a specialized instanced draw call
	};
	V		cpu_data[6] = {
		{ v2(+1,-1) },
		{ v2(+1,+1) },
		{ v2(-1,-1) },
		
		{ v2(-1,-1) },
		{ v2(+1,+1) },
		{ v2(-1,+1) },
	};
	
	void init_static () {
		vbo = vbo_init_and_upload_static(cpu_data, sizeof(cpu_data));
	}
	void bind (Basic_Shader cr shad) {
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		
		GLint	pos_clip = glGetAttribLocation(shad.prog, "attrib_pos_clip");
		
		glEnableVertexAttribArray(pos_clip);
		glVertexAttribPointer(pos_clip,	2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,pos_clip));
		
	}
	
};
struct Shader_Quad_Particle : Basic_Shader {
	Shader_Quad_Particle (): Basic_Shader(
// Vertex shader
R"_SHAD(
	attribute	vec2	attrib_pos_clip;
	varying		vec3	color;
	
	uniform		mat4	world_to_clip;
	uniform		vec2	particle_pos_world;
	uniform		vec2	particle_scale_world;
	uniform		vec3	particle_col;
	
	void main() {
		gl_Position = world_to_clip * vec4((attrib_pos_clip * particle_scale_world +particle_pos_world), 0.0, 1.0);
		color = particle_col;
	}
)_SHAD",
// Fragment shader
R"_SHAD(
	varying		vec3	color;
	
	void main() {
		gl_FragColor = vec4(color, 1.0);
	}
)_SHAD"
	) {}
	
	Unif_fm4	unif_world_to_clip;
	Unif_fv3	unif_particle_col;
	Unif_fv2	unif_particle_scale_world;
	Unif_fv2	unif_particle_pos_world;
	
	void init () {
		compile();
		
		unif_world_to_clip.loc =			glGetUniformLocation(prog, "world_to_clip");
		unif_particle_pos_world.loc =		glGetUniformLocation(prog, "particle_pos_world");
		unif_particle_scale_world.loc =		glGetUniformLocation(prog, "particle_scale_world");
		unif_particle_col.loc =				glGetUniformLocation(prog, "particle_col");
		
	}
};
#endif

struct VBO_Pos_Col {
	GLuint	vbo;
	struct V {
		v2	pos;
		v4	col;
	};
	
	void init () {
		glGenBuffers(1, &vbo);
	}
	void upload (array<V> cr data) {
		uptr data_size = data.len * sizeof(V);
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferData(GL_ARRAY_BUFFER, data_size, NULL, GL_STATIC_DRAW);
		glBufferData(GL_ARRAY_BUFFER, data_size, data.arr, GL_STATIC_DRAW);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	void bind (Basic_Shader cr shad) {
		
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		
		GLint	pos =	glGetAttribLocation(shad.prog, "attrib_pos");
		GLint	col =	glGetAttribLocation(shad.prog, "attrib_col");
		
		glEnableVertexAttribArray(pos);
		glVertexAttribPointer(pos,	2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,pos));
		
		glEnableVertexAttribArray(col);
		glVertexAttribPointer(col,	4, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,col));
		
	}
	
};
struct Shader_Clip_Col : Basic_Shader {
	Shader_Clip_Col (): Basic_Shader(
// Vertex shader
R"_SHAD(
	attribute	vec2	attrib_pos; // clip
	attribute	vec4	attrib_col;
	varying		vec4	color;
	
	void main() {
		gl_Position = vec4(attrib_pos, 0.0, 1.0);
		color = attrib_col;
	}
)_SHAD",
// Fragment shader
R"_SHAD(
	varying		vec4	color;
	
	void main() {
		gl_FragColor = color, 1.0;
	}
)_SHAD"
	) {}
	
	void init () {
		compile();
	}
};
struct Shader_World_Col : Basic_Shader {
	Shader_World_Col (): Basic_Shader(
// Vertex shader
R"_SHAD(
	attribute	vec2	attrib_pos; // world
	attribute	vec4	attrib_col;
	varying		vec4	color;
	uniform		mat4	world_to_clip;
	
	void main() {
		gl_Position = world_to_clip * vec4(attrib_pos, 0.0, 1.0);
		color = attrib_col;
	}
)_SHAD",
// Fragment shader
R"_SHAD(
	varying		vec4	color;
	
	void main() {
		gl_FragColor = color, 1.0;
	}
)_SHAD"
	) {}
	
	// uniforms
	Unif_fm4	world_to_clip;
	
	void init () {
		compile();
		
		world_to_clip.loc =		glGetUniformLocation(prog, "world_to_clip");
		
	}
};

#if 0
struct Func_Graph {
	VBO_Clip_Col			vbo;
	typedef				f32 (*func_fp)(f32);
	
	void init () {
		vbo.init();
		vbo_data.arr = nullptr; // to allow 'free(); alloc();'
	}
	template <typename LAMBDA>
	void generate (LAMBDA func, v2 data_domain_min, v2 data_domain_max, u32 samples,
			v2 graph_pos, v2 graph_dim) { // graph pos & dim in percent of screen from lower left
		dbg_assert(samples >= 2);
		
		graph_pos = graph_pos * 2 -1;
		graph_dim = graph_dim * 2;
		
		auto vbo_data = array<VBO_Clip_Col::V>::malloc(6 +samples);
		defer { vbo_data.free(); };
		
		v4 bg_col = v4(1,1,1, 0.01f);
		vbo_data[0] = { graph_pos +graph_dim*v2(1,0), bg_col };
		vbo_data[1] = { graph_pos +graph_dim*v2(1,1), bg_col };
		vbo_data[2] = { graph_pos +graph_dim*v2(0,0), bg_col };
		vbo_data[3] = { graph_pos +graph_dim*v2(0,0), bg_col };
		vbo_data[4] = { graph_pos +graph_dim*v2(1,1), bg_col };
		vbo_data[5] = { graph_pos +graph_dim*v2(0,1), bg_col };
		
		for (u32 i=0; i<samples; ++i) {
			f32 x_01 = (f32)i / (f32)(samples -1); // map x from [0,1] with even spaced steps inbetween
			f32 x = lerp(data_domain_min.x, data_domain_max.x, x_01); // map to x domain
			f32 y = func(x);
			f32 y_01 = map(y, data_domain_min.y, data_domain_max.y);
			
			
			vbo_data[6 +i].pos_clip = graph_pos +graph_dim*v2(x_01,y_01);
			vbo_data[6 +i].col = v4(i%2?0:1,i%2?1:0,0,1);
		}
		
		vbo.upload(vbo_data);
	}
	void draw (Basic_Shader cr shad) {
		vbo.bind(shad);
		
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDrawArrays(GL_LINE_STRIP, 6, (s32)vbo_data.len-6);
	}
};
#endif

struct Particle_Sim {
	Shader_World_Col	shad_world_col;
	VBO_Pos_Col			bg_quad_vbo;
	VBO_Pos_Col			paricles_vbo;
	
	struct Particle {
		v2	pos;
		v2	vel;
		f32	mass;
		v3	col;
	};
	v2 world_radius = v2(100);
	
	Particle particles[140];
	u32 particle_count = arrlenof(u32, particles);
	
	void init  () {
		cam = { v2(0), MAX(world_radius.x, world_radius.y)*1.1f };
		
		glfwSetWindowPos(wnd, 1920-1000 -16, 35);
		glfwShowWindow(wnd);
		
		shad_world_col.init();
		bg_quad_vbo.init();
		paricles_vbo.init();
		
		for (u32 i=0; i<particle_count; ++i) {
			f32 t = (f32)i / (f32)(particle_count -1);
			
			#if 0
			particles[i].pos = random::v2_n1p1() * world_radius;
			particles[i].vel = random::v2_n1p1() * v2(5.0f);
			#else
			//f32 r = world_radius.x * lerp(0.7f, 0.8f, random::f32_01());
			f32 r = world_radius.x * 0.75f;
			
			particles[i].pos = rotate2(t*deg(360)) * v2(r, 0);
			particles[i].vel = rotate2(t*deg(360)) * v2(0,+22.0f);
			#endif
			
			particles[i].mass = 0.5f;
			particles[i].col = random::v3_01() * v3(0.5f) +v3(0.5f);
		}
		
	}
	
	void frame (f64 t, f32 dt) {
		
		v2 total_vel;
		
		u32 tsteps = 16;
		u32 taccel = 1;
		for (u32 tstep=0; tstep<tsteps*taccel; ++tstep) {
			//dbg_assert(dt >= 0.0f);
			//dt = MIN(dt, 1.0f / 16);
			//dt /= 4.0f;
			dt = 1.0f / (60.0f*tsteps);
			
			total_vel = v2(0);
			for (u32 i=0; i<particle_count; ++i) {
				auto& p = particles[i];
				
				//f32 wall_force_scale = 100 * 100;
				//f32 wall_l_force = lerp(wall_force_scale,0, step(-world_radius.x-4,-world_radius.x+1, p.pos.x));
				//f32 wall_r_force = lerp(wall_force_scale,0, step(+world_radius.x+4,+world_radius.x-1, p.pos.x));
				//f32 wall_t_force = lerp(wall_force_scale,0, step(-world_radius.y-4,-world_radius.y+1, p.pos.y));
				//f32 wall_b_force = lerp(wall_force_scale,0, step(+world_radius.y+4,+world_radius.y-1, p.pos.y));
				//v2 wall_force = v2(	+wall_l_force + -wall_r_force,
				//					+wall_t_force + -wall_b_force );
				v2 grav_force = v2(0);
				{
					for (u32 i=0; i<particle_count; ++i) {
						v2 dir = particles[i].pos -p.pos;
						f32 dist = length(dir);
						if (dist < 1.7f) continue;
						dir = dir / v2(dist);
						
						grav_force += dir * v2(200.0f / (dist*dist));
					}
				}
				
				v2 force = /*wall_force +*/grav_force;
				
				v2 massv = v2(p.mass);
				v2 accel = force / massv;
				
				p.vel += accel * v2(dt);
				p.pos += p.vel * v2(dt);
				
				total_vel += p.vel;
			}
		}
		//printf("total vel: %f {%f,%f}\n", length(total_vel), total_vel.x, total_vel.y);
		
		v4 world_rect_col = v4( srgb(41,49,52), 1 );
		v3 background_col = world_rect_col.xyz() * v3(0.8f);
		
		glViewport(0, 0, wnd_dim.x, wnd_dim.y);
		
		glClearColor(background_col.x, background_col.y, background_col.z, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		
		shad_world_col.bind();
		shad_world_col.world_to_clip.set( cam.world_to_clip );
		
		{ // world quad highlighted a bit
			v4 col = world_rect_col;
			
			VBO_Pos_Col::V quad[6] = {
				{ world_radius*v2(+1,-1), col },
				{ world_radius*v2(+1,+1), col },
				{ world_radius*v2(-1,-1), col },
				{ world_radius*v2(-1,-1), col },
				{ world_radius*v2(+1,+1), col },
				{ world_radius*v2(-1,+1), col },
			};
			
			bg_quad_vbo.upload({quad,6});
			bg_quad_vbo.bind(shad_world_col);
			
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		
		{
			auto particles_data = array<VBO_Pos_Col::V>::malloc(particle_count);
			defer { particles_data.free(); };
			
			for (u32 i=0; i<particle_count; ++i) {
				particles_data[i].pos = particles[i].pos;
				particles_data[i].col = v4(particles[i].col, 1);
			}
			
			paricles_vbo.upload(particles_data);
			paricles_vbo.bind(shad_world_col);
			
			glDrawArrays(GL_POINTS, 0, particles_data.len);
		}
	}
	
};

#if 0
struct Spring_Test {
	Shader_Quad_Particle	shad;
	VBO_Particle			vbo;
	
	Shader_Clip_Col				graph_shad;
	Func_Grapher				graph;
	
	struct Particle {
		v2	pos;
		v2	vel;
		f32	mass;
		v3	col;
	};
	v2 world_radius = v2(100);
	
	Particle euler;
	Particle euler_min;
	Particle euler_max;
	
	Particle euler2;
	Particle euler2_min;
	Particle euler2_max;
	
	Particle euler_ref;
	Particle euler_ref_min;
	Particle euler_ref_max;
	
	void init  () {
		cam = { v2(0), MAX(world_radius.x, world_radius.y)*1.1f };
		
		glfwSetWindowPos(wnd, 16, 35);
		glfwShowWindow(wnd);
		
		shad.init();
		vbo.init_static();
		
		graph_shad.init();
		graph.init();
		
		euler.pos = v2(0, +0.5f * world_radius.y);
		euler.vel = v2(+300,0);
		euler.mass = 1;
		euler.col = v3(1,0.2f,0.2f);
		
		euler_min.col = v3(1,0.1f,0.1f);
		euler_max.col = v3(1,0.1f,0.1f);
		
		
		euler2.pos = v2(0, 0.0f * world_radius.y);
		euler2.vel = v2(+300,0);
		euler2.mass = 1;
		euler2.col = v3(0.2f,1,0.2f);
		
		euler2_min.col = v3(0.1f,1,0.1f);
		euler2_max.col = v3(0.1f,1,0.1f);
		
		euler_ref.pos = v2(0, -0.5f * world_radius.y);
		euler_ref.mass = 1;
		euler_ref.col = v3(0.2f,0.2f,1);
		
		euler_ref_min.col = v3(0.1f,0.1f,1);
		euler_ref_max.col = v3(0.1f,0.1f,1);
		
	}
	
	void frame (f64 t, f32 dt) {
		dt = 1.0f/(60.0f +random::float_01()*3);
		
		f32 k = 1 ? 1000 : 39.4784176043f;
		f32 r = world_radius.x;
		//f32 r = 0;
		
		auto spring_model_euler = [&] (Particle& p) {
			p.pos += p.vel * v2(dt);
			
			f32 accel_l = p.pos.x >= -r ? 0 : -(p.pos.x +r) * k;
			f32 accel_r = p.pos.x <= +r ? 0 : -(p.pos.x -r) * k;
			v2 accel = v2(accel_l +accel_r, 0);
			p.vel += accel * v2(dt);
		};
		spring_model_euler(euler);
		euler_min.pos = v2(MIN(euler_min.pos.x, euler.pos.x), euler.pos.y -5);
		euler_max.pos = v2(MAX(euler_max.pos.x, euler.pos.x), euler.pos.y -5);
		
		auto spring_model_euler2 = [&] (Particle& p) {
			f32 accel_l = p.pos.x >= -r ? 0 : -(p.pos.x +r) * k;
			f32 accel_r = p.pos.x <= +r ? 0 : -(p.pos.x -r) * k;
			v2 accel = v2(accel_l +accel_r, 0);
			p.vel += accel * v2(dt);
			
			p.pos += p.vel * v2(dt);
		};
		spring_model_euler2(euler2);
		euler2_min.pos = v2(MIN(euler2_min.pos.x, euler2.pos.x), euler2.pos.y -5);
		euler2_max.pos = v2(MAX(euler2_max.pos.x, euler2.pos.x), euler2.pos.y -5);
		
		auto spring_model_euler_ref = [&] (Particle& p) {
			
			f64 vmax = 300.0;
			f64 spring_half_period = PId * sqrt(1.0 / (f64)k);
			f64 linear_t = world_radius.x*2 / vmax;
			f64 half_period = linear_t +spring_half_period;
			f64 full_period = half_period*2;
			
			f64 w = 1.0 / sqrt(1.0 / (f64)k);
			f64 amp = (spring_half_period*2 * vmax) / TAUd;
			
			auto calc = [&] (f32 t) -> f32 {
				f64 t_ = mod((f64)t +linear_t/2, full_period);
				f64 dir;
				if (t_ <= half_period) {
					dir = +1;
				} else {
					dir = -1;
					t_ -= half_period;
				}
				
				f64 posx;
				if (t_ < linear_t) {
					posx = vmax * t_ -world_radius.x;
				} else {
					t_ -= linear_t;
					posx = amp * sin(t_ * w) +world_radius.x;
				}
				
				posx = posx * dir;
				return (f32)posx;
			};
			graph.generate(calc, v2(0, -world_radius.x), v2((f32)full_period, +world_radius.x), 100,
					v2(0.125f, 0.70f), v2(0.75f, 0.25f));
			
			//p.vel = ;
			p.pos.x = calc((f32)t);
		};
		spring_model_euler_ref(euler_ref);
		euler_ref_min.pos = v2(MIN(euler_ref_min.pos.x, euler_ref.pos.x), euler_ref.pos.y -5);
		euler_ref_max.pos = v2(MAX(euler_ref_max.pos.x, euler_ref.pos.x), euler_ref.pos.y -5);
		
		#if 0
		printf("euler: %f %f, {%.2f,%.2f}, %.2f}\n",
			euler_min.pos.x, euler_max.pos.x,
			euler.pos.x,euler.pos.y, euler.vel.x);
			
		printf("euler2: %f %f, {%.2f,%.2f}, %.2f}\n",
			euler2_min.pos.x, euler2_max.pos.x,
			euler2.pos.x,euler2.pos.y, euler2.vel.x);
		#endif
		
		printf("%f %f %f\n", euler_min.pos.x, euler2_min.pos.x, euler_ref_min.pos.x);
		
		v3 world_rect_col = srgb(41,49,52);
		v3 background_col = world_rect_col * v3(0.8f);
		
		glViewport(0, 0, wnd_dim.x, wnd_dim.y);
		
		glClearColor(background_col.x, background_col.y, background_col.z, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		
		shad.bind();
		vbo.bind(shad);
		shad.unif_world_to_clip.set(cam.world_to_clip);
		
		{ // world quad highlighted a bit
			shad.unif_particle_scale_world.set(world_radius);
			shad.unif_particle_pos_world.set(v2(0));
			shad.unif_particle_col.set(world_rect_col);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		
		shad.unif_particle_scale_world.set(v2(1));
		
		auto draw = [&] (Particle cr p) {
			shad.unif_particle_pos_world.set(p.pos);
			shad.unif_particle_col.set(p.col);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		};
		draw(euler);
		draw(euler_min);
		draw(euler_max);
		
		draw(euler2);
		draw(euler2_min);
		draw(euler2_max);
		
		draw(euler_ref);
		draw(euler_ref_min);
		draw(euler_ref_max);
		
		graph_shad.bind();
		graph.draw(graph_shad);
	}
	
};
#endif

int main (int argc, char** argv) {
	
	random::init_same_seed_everytime();
	
	setup_glfw();
	
	glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glfwSwapInterval(1);
	
	#if 0
	Spring_Test game = Spring_Test();
	#else
	Particle_Sim game = Particle_Sim();
	#endif
	
	game.init();
	
	bool	dragging = false;
	v2		dragging_grab_pos_world;
	
	f32	dt = 0;
	u64	initial_ts = glfwGetTimerValue();
	u64	prev_frame_end = initial_ts;
	
	for (frame_indx=0;; ++frame_indx) {
		f64 t = (f64)(prev_frame_end -initial_ts) / glfwGetTimerFrequency();
		
		scrollwheel_diff = 0;
		buttons_reset_toggle_count();
		
		glfwPollEvents();
		
		if (glfwWindowShouldClose(wnd)) break;
		
		{
			glfwGetFramebufferSize(wnd, &wnd_dim.x, &wnd_dim.y);
			dbg_assert(wnd_dim.x > 0 && wnd_dim.y > 0);
			
			v2 tmp = (v2)wnd_dim;
			wnd_dim_aspect = tmp / v2(tmp.y, tmp.x);
		}
		
		{
			dv2 pos;
			glfwGetCursorPos(wnd, &pos.x, &pos.y);
			cursor_pos = iv2( (s32)floor(pos.x), (s32)floor(pos.y) );
		}
		
		{
			auto calc_world_to_clip = [] () {
				f32 radius_scale = 1.0f / cam.radius;
				v2 scale;
				if (wnd_dim.x > wnd_dim.y) {
					scale = v2(wnd_dim_aspect.y * radius_scale, radius_scale);
				} else {
					scale = v2(radius_scale, wnd_dim_aspect.x * radius_scale);
				}
				
				cam.world_to_clip = scale4(v3(scale, 1)) * translate4(v3(-cam.pos_world, 0));
			};
			
			bool cursor_in_wnd =	   cursor_pos.x >= 0 && cursor_pos.x < wnd_dim.x
									&& cursor_pos.y >= 0 && cursor_pos.y < wnd_dim.y;
			
			{
				f32 tmp = log2(cam.radius);
				tmp += (f32)scrollwheel_diff * -0.1f;
				cam.radius = pow(2.0f, tmp);
			}
			
			calc_world_to_clip(); // to fix problem with zooming and dragging at the same time
			
			if (!dragging) {
				if (cursor_in_wnd && button_is_down(B_RMB)) {
					
					m2 clip_to_cam2 = inverse( cam.world_to_clip.m2() );
					
					v2 clip = ((v2)cursor_pos / (v2)wnd_dim -0.5f) * v2(2,-2); // mouse cursor points to upper left corner of pixel, so no pixel center calculation here
					dragging_grab_pos_world = clip_to_cam2 * clip +cam.pos_world;
					
					dragging = true;
				}
			} else {
				if (button_is_up(B_RMB)) {
					dragging = false;
				}
				
				m2 clip_to_cam2 = inverse( cam.world_to_clip.m2() );
				
				v2 clip = ((v2)cursor_pos / (v2)wnd_dim -0.5f) * v2(2,-2); // mouse cursor points to upper left corner of pixel, so no pixel center calculation here
				
				cam.pos_world = -(clip_to_cam2 * clip) +dragging_grab_pos_world; // TODO: seems to have what i think is 1 frame of latency even without vsync (at 17-fps), is this the desktop compistor or do i have a latency in my calculation?
				
				//printf(">>> %f %f\n", cam_.x,cam_.y);
			}
			
			calc_world_to_clip();
			
		}
		
		game.frame(t, dt);
		
		glfwSwapBuffers(wnd);
		
		{
			u64 now = glfwGetTimerValue();
			dt = (f32)(now -prev_frame_end) / (f32)glfwGetTimerFrequency();
			prev_frame_end = now;
		}
	}
	
	glfwDestroyWindow(wnd);
	glfwTerminate();
	
	return 0;
}
