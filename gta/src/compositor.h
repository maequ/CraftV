// The ReShade add-on half of CraftV.asi (compositor.cpp): uploads Minecraft's latest frame (PROTOCOL.md §11) into
// textures that CraftV.fx composites into GTA's picture against GTA's depth buffer. Adapted from
// minecraft-gta5-passthrough (rehan-remade, MIT).
#pragma once

namespace compositor
{
	/// Registers with ReShade once it is loaded (ReShade64.asi, loaded by the ASI loader); call until it returns true.
	bool try_register(void *module);
	/// Whether ReShade loaded us: the passthrough is possible.
	bool registered();
	void unregister(void *module);
	/// Whether to composite this frame (the plugin turns it off in menus, cutscenes and when the passthrough is off).
	void set_active(bool active);
	/// GTA's camera clip planes, to turn its depth buffer into metres.
	void set_host_planes(float near_clip, float far_clip);
	/// GTA's current camera in Minecraft's convention (rotation in degrees, vertical fov, position in Minecraft
	/// coordinates): Minecraft's frame is re-projected from the pose it was rendered with to this one, hiding the
	/// link's latency on camera turns and moves.
	void set_host_pose(float yaw, float pitch, float roll, float fov, double x, double y, double z);
	/// How many poses back the presented picture is (the reference measured 0 as best).
	void set_pose_lag(int frames);
	/// Composite Minecraft's picture as rendered, without re-projecting it to GTA's newer pose.
	void set_camera_locked(bool locked);
	/// GTA's backbuffer size as ReShade sees it (0 until the first frame).
	void backbuffer_size(int &width, int &height);
}
