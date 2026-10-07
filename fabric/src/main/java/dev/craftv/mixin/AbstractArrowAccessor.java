package dev.craftv.mixin;

import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.gen.Accessor;
import org.spongepowered.asm.mixin.gen.Invoker;

/** A flying arrow's state for the GTA players' games (GtaWorld): stuck or flying, and how hard it hits. */
@Mixin(AbstractArrow.class)
public interface AbstractArrowAccessor {
	@Invoker("isInGround")
	boolean craftv$isInGround();

	@Accessor("baseDamage")
	double craftv$getBaseDamage();
}
