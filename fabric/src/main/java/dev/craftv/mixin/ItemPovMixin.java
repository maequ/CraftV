package dev.craftv.mixin;

import dev.craftv.OwnerAim;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.ClipContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * Buckets (and other items that aim themselves) look from the player's eyes along the player's rotation. In the
 * passthrough's third person the owner's head faces where they walk, not where the camera looks, so water went
 * nowhere. For the owner, aim along the host camera's ray instead (OwnerAim), on the client and the server alike.
 */
@Mixin(Item.class)
abstract class ItemPovMixin {
	private static final double BACK_OFF = 0.4;

	@Inject(method = "getPlayerPOVHitResult", at = @At("HEAD"), cancellable = true)
	private static void craftv$cameraRay(Level level, Player player, ClipContext.Fluid fluid, CallbackInfoReturnable<BlockHitResult> cir) {
		OwnerAim.Aim aim = OwnerAim.of(player);
		if (aim == null) {
			return;
		}
		Vec3 eye = player.getEyePosition();
		double along = Math.max(0.0, eye.subtract(aim.from()).dot(aim.dir()));
		Vec3 start = aim.from().add(aim.dir().scale(Math.max(0.0, along - BACK_OFF)));
		Vec3 end = aim.from().add(aim.dir().scale(along + player.blockInteractionRange()));
		cir.setReturnValue(level.clip(new ClipContext(start, end, ClipContext.Block.OUTLINE, fluid, player)));
	}
}
