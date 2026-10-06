package dev.craftv.mixin;

import dev.craftv.coop.CoopServer;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.LivingEntity;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * The owner's body is the host game's player, and the host game already handles their falls and swimming: a GTA
 * parachute jump or a car landing a jump would otherwise be a deadly Minecraft fall, and swimming in GTA puts the owner
 * under CraftV's terrain water. So the owner takes no Minecraft fall damage and never drowns. Friends are unaffected.
 */
@Mixin(LivingEntity.class)
abstract class OwnerSafetyMixin {
	private static boolean craftv$isOwner(Object self) {
		return self instanceof ServerPlayer p && p.level().getServer() != null && CoopServer.isOwner(p.level().getServer(), p);
	}

	@Inject(method = "causeFallDamage", at = @At("HEAD"), cancellable = true)
	private void craftv$noOwnerFallDamage(double fallDistance, float multiplier, DamageSource source, CallbackInfoReturnable<Boolean> cir) {
		if (craftv$isOwner(this)) {
			cir.setReturnValue(false);
		}
	}

	@Inject(method = "canBreatheUnderwater", at = @At("HEAD"), cancellable = true)
	private void craftv$ownerBreathes(CallbackInfoReturnable<Boolean> cir) {
		if (craftv$isOwner(this)) {
			cir.setReturnValue(true);
		}
	}
}
