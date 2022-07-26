R"(
[nzsl_version("1.0")]
module;

[layout(std140)]
struct Data
{
	halfScreenWidth : f32,
	halfScreenHeight : f32,
}

[set(0)]
external
{
	[binding(0)] data: uniform[Data]
}

struct VertIn
{
	[location(0)] position: vec3[f32],
	[location(1)] color: vec4[f32],
	[location(2)] uv: vec2[f32],
}

struct VertOut
{
	[builtin(position)] position: vec4[f32],
	[location(0)] color: vec4[f32],
	[location(1)] uv: vec2[f32]
}

struct FragOut
{
	[location(0)] color: vec4[f32]
}

[entry(frag)]
fn main(fragIn: VertOut) -> FragOut
{
	let fragOut: FragOut;
    fragOut.color = fragIn.color;
    fragOut.color = fragOut.color;
	return fragOut;
}

[entry(vert)]
fn main(vertIn: VertIn) -> VertOut
{
	let vertOut: VertOut;
	vertOut.position = vec4[f32](vertIn.position, 1.0) / vec4[f32](data.halfScreenWidth, data.halfScreenHeight, 1.0, 1.0) - vec4[f32](1.0,1.0,0.0,0.0);
	vertOut.color = vertIn.color;
	vertOut.uv = vertIn.uv;
	return vertOut;
}
)"