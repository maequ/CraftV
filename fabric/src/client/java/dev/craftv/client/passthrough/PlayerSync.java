package dev.craftv.client.passthrough;

import dev.craftv.link.Messages;
import net.minecraft.client.CameraType;
import net.minecraft.client.Minecraft;
import net.minecraft.client.player.LocalPlayer;

/**
 * Keeps the owner on the host's player at frame rate while the passthrough is on: in first person their eyes
 * are at the host camera, in third person their feet are where the host's player stands and they face the
 * host's body. (PlayerPuppet still applies PLAYER_STATE every tick, which is what the server sees.) Adapted
 * from minecraft-gta5-passthrough (rehan-remade, MIT).
 */
public final class PlayerSync {
	private static float tickDistance;
	private static double lastX = Double.NaN, lastZ;

	private PlayerSync() {
	}

	/** How far the owner moved over the last client tick (the position is set every frame, so vanilla sees ~0). */
	public static float tickDistance() {
		return tickDistance;
	}

	/** Every frame, before the camera update. */
	public static void frame() {
		Messages.Camera c = HostCamera.frame();
		Minecraft minecraft = Minecraft.getInstance();
		LocalPlayer player = minecraft.player;
		if (c == null || player == null) {
			return;
		}
		player.setYRot(c.yaw());
		player.setXRot(c.pitch());
		player.yRotO = c.yaw();
		player.xRotO = c.pitch();
		player.yHeadRot = player.yHeadRotO = c.yaw();
		player.yBodyRot = player.yBodyRotO = c.firstPerson() ? c.yaw() : c.bodyYaw();
		double x = c.firstPerson() ? c.x() : c.feetX();
		double y = c.firstPerson() ? c.y() - player.getEyeHeight() : c.feetY();
		double z = c.firstPerson() ? c.z() : c.feetZ();
		player.setPos(x, y, z);
		player.xo = player.xOld = x;
		player.yo = player.yOld = y;
		player.zo = player.zOld = z;
		CameraType type = c.firstPerson() ? CameraType.FIRST_PERSON : CameraType.THIRD_PERSON_BACK;
		if (minecraft.options.getCameraType() != type) {
			minecraft.options.setCameraType(type);
		}
	}

	/** Every client tick: the distance walked, for the walk animation. */
	public static void tick() {
		Messages.Camera c = HostCamera.live();
		if (c == null) {
			lastX = Double.NaN;
			tickDistance = 0;
			return;
		}
		tickDistance = Double.isNaN(lastX) ? 0.0F : (float) Math.min(Math.hypot(c.feetX() - lastX, c.feetZ() - lastZ), 1.0);
		lastX = c.feetX();
		lastZ = c.feetZ();
	}
}
