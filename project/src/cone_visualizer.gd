## Renders a polar directivity diagram on the ground around the speaker.
## Shape matches the actual Steam Audio directivity formula:
##   factor = |(1 - dipole_weight) + dipole_weight * cos(angle)|^dipole_power
## Forward direction = speaker's local -Z axis.
extends MeshInstance3D

@export var dipole_weight: float = 0.0
@export var dipole_power: float = 1.0
@export var max_radius: float = 4.0
@export var color: Color = Color(0.8, 0.8, 0.8, 0.55)
@export var segments: int = 128

func _ready() -> void:
	var arr_mesh := _build_mesh()
	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.vertex_color_use_as_albedo = true
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.no_depth_test = false
	arr_mesh.surface_set_material(0, mat)
	mesh = arr_mesh

func _directivity(angle_rad: float) -> float:
	var d := (1.0 - dipole_weight) + dipole_weight * cos(angle_rad)
	return pow(maxf(d, 0.0), dipole_power)

func _build_mesh() -> ArrayMesh:
	var verts := PackedVector3Array()
	var cols := PackedColorArray()
	var indices := PackedInt32Array()

	# Raise slightly above ground to avoid z-fighting
	const Y := 0.02

	# Center vertex — slightly dimmer
	verts.append(Vector3(0.0, Y, 0.0))
	cols.append(Color(color.r, color.g, color.b, color.a * 0.25))

	for i in range(segments + 1):
		var angle := (float(i) / float(segments)) * TAU
		var factor := _directivity(angle)
		var r := factor * max_radius
		# angle=0 → straight ahead = speaker's local -Z
		var x := sin(angle) * r
		var z := -cos(angle) * r
		verts.append(Vector3(x, Y, z))
		cols.append(Color(color.r, color.g, color.b, factor * color.a))

	for i in range(segments):
		indices.append(0)
		indices.append(i + 1)
		indices.append(i + 2)

	var arr := Array()
	arr.resize(Mesh.ARRAY_MAX)
	arr[Mesh.ARRAY_VERTEX] = verts
	arr[Mesh.ARRAY_COLOR] = cols
	arr[Mesh.ARRAY_INDEX] = indices

	var arr_mesh := ArrayMesh.new()
	arr_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arr)
	return arr_mesh
