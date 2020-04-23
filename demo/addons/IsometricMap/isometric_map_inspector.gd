tool
extends EditorInspectorPlugin

class_name IsometricMapInspector

var undo_redo: UndoRedo

func can_handle(object):
	return object is IsoMap

func parse_property(object, type, path, hint, hint_text, usage):
	if path == "map_size":
		add_property_editor(path, MapSizeEditor.new(undo_redo))
		return true
	return false

func _init(undoredo: UndoRedo):
	undo_redo = undoredo

class MapSizeEditor extends EditorProperty:
	var updating = false
	var terrain_types: Dictionary = Dictionary()
	var widget = load("res://addons/IsometricMap/widget/MapSizeWidget.tscn").instance()
	var undo_redo: UndoRedo

	func update_property():
		var new_value = get_edited_object()[ get_edited_property() ]
		updating=true
		for i in terrain_types.keys():
			var value = terrain_types[i] as PlaceholderType
			if value == new_value:
				#vbox.select(i)
				break
		updating=false

	func _init(undoredo: UndoRedo):
		undo_redo = undoredo
		add_child(widget)
		widget.get_node("xle").connect("text_entered", self, "change_map_width")
		widget.get_node("yle").connect("text_entered", self, "change_map_depth")
		widget.get_node("zle").connect("text_entered", self, "change_map_height")
	
	func _ready():
		var iso_map: IsoMap = get_edited_object() as IsoMap
		widget.get_node("xle").text = str(iso_map.map_size.x)
		widget.get_node("yle").text = str(iso_map.map_size.y)
		widget.get_node("zle").text = str(iso_map.map_size.z)
	
	func change_map_width(value: String) -> void:
		var iso_map: IsoMap = get_edited_object() as IsoMap
		var map_size: = Vector3(int(value), iso_map.map_size.y, iso_map.map_size.z)
		undo_redo.create_action("change_map_width")
		remove_out_of_range_placeholders(map_size)
		undo_redo.add_do_property(iso_map, "map_size", map_size)
		undo_redo.add_undo_property(iso_map, "map_size", iso_map.map_size)
		undo_redo.commit_action()
	
	func change_map_depth(value: String) -> void:
		var iso_map: IsoMap = get_edited_object() as IsoMap
		var map_size: = Vector3(iso_map.map_size.x, int(value), iso_map.map_size.z)
		undo_redo.create_action("change_map_depth")
		remove_out_of_range_placeholders(map_size)
		undo_redo.add_do_property(iso_map, "map_size", map_size)
		undo_redo.add_undo_property(iso_map, "map_size", iso_map.map_size)
		undo_redo.commit_action()
	
	func change_map_height(value: String) -> void:
		var iso_map: IsoMap = get_edited_object() as IsoMap
		var map_size: = Vector3(iso_map.map_size.x, iso_map.map_size.y, int(value))
		undo_redo.create_action("change_map_height")
		remove_out_of_range_placeholders(map_size)
		undo_redo.add_do_property(iso_map, "map_size", map_size)
		undo_redo.add_undo_property(iso_map, "map_size", iso_map.map_size)
		undo_redo.commit_action()
	
	func remove_out_of_range_placeholders(map_size: Vector3) -> void:
		var iso_map: IsoMap = get_edited_object() as IsoMap
		for pl in iso_map.get_children():
			if pl is BasePlaceholder:
				var plhd: = pl as BasePlaceholder
				if plhd.ortho_position.x + plhd.width > map_size.x or plhd.ortho_position.y + plhd.depth > map_size.y or plhd.ortho_position.z + plhd.height > map_size.z:
					undo_redo.add_do_method(iso_map, "remove_placeholder", plhd, plhd.ortho_position)
					undo_redo.add_undo_method(iso_map, "add_placeholder", plhd, plhd.ortho_position)
					undo_redo.add_undo_method(iso_map, "add_placeholder_as_child", plhd)