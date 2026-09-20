#include "elemental/reaction.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    }
}

void near(double actual, double expected, double tolerance, std::string_view message) {
    if (std::abs(actual - expected) > tolerance) {
        std::cerr << "[FAIL] " << message << ": expected " << expected << ", got " << actual
                  << '\n';
        ++failures;
    }
}

elemental::ActorStats actor(std::string_view id, double em = 0.0) {
    elemental::ActorStats value;
    value.id = std::string(id);
    value.level = 90;
    value.elemental_mastery = em;
    return value;
}

elemental::Attack aura_attack(
    elemental::Element element,
    double gauge,
    const elemental::ActorStats& source) {
    elemental::Attack value;
    value.element = element;
    value.gauge_units = gauge;
    value.source = source;
    return value;
}

bool has_reaction(const elemental::Resolution& result, elemental::Reaction reaction) {
    for (const auto& event : result.events) {
        if (event.reaction == reaction) {
            return true;
        }
    }
    return false;
}

void aura_tax_and_decay() {
    elemental::ReactionEngine engine;
    elemental::TargetState target;
    const auto pyro = actor("pyro");

    const auto result = engine.apply(target, aura_attack(elemental::Element::pyro, 1.0, pyro));
    check(result.events.empty(), "first Element should only establish an aura");
    near(target.aura_gauge(elemental::Element::pyro), 0.8, 1.0e-9, "1U aura tax");
    near(target.aura_remaining_seconds(elemental::Element::pyro), 9.5, 1.0e-9, "1U duration");

    (void)engine.advance(target, 4.75);
    near(target.aura_gauge(elemental::Element::pyro), 0.4, 1.0e-9, "linear aura decay");

    (void)engine.apply(target, aura_attack(elemental::Element::pyro, 2.0, pyro));
    near(target.aura_gauge(elemental::Element::pyro), 1.6, 1.0e-9, "stronger aura refresh");
    near(
        target.aura_remaining_seconds(elemental::Element::pyro),
        12.0,
        1.0e-9,
        "Pyro refresh adopts the new decay rate when its gauge changes");
}

void vaporize_directions_and_consumption() {
    elemental::ReactionEngine engine;
    const auto hydro = actor("hydro");
    const auto pyro = actor("pyro");

    elemental::TargetState reverse_target;
    (void)engine.apply(reverse_target, aura_attack(elemental::Element::hydro, 1.0, hydro));
    const auto reverse =
        engine.apply(reverse_target, aura_attack(elemental::Element::pyro, 1.0, pyro));
    check(has_reaction(reverse, elemental::Reaction::vaporize), "Pyro on Hydro should Vaporize");
    near(reverse.attack_multiplier, 1.5, 1.0e-9, "reverse Vaporize multiplier");
    near(
        reverse_target.aura_gauge(elemental::Element::hydro),
        0.3,
        1.0e-9,
        "reverse Vaporize consumes 0.5U");

    elemental::TargetState forward_target;
    (void)engine.apply(forward_target, aura_attack(elemental::Element::pyro, 1.0, pyro));
    const auto forward =
        engine.apply(forward_target, aura_attack(elemental::Element::hydro, 1.0, hydro));
    near(forward.attack_multiplier, 2.0, 1.0e-9, "forward Vaporize multiplier");
    near(
        forward_target.aura_gauge(elemental::Element::pyro),
        0.0,
        1.0e-9,
        "forward Vaporize consumes 2U");
}

void electro_charged_coexistence_and_ticks() {
    elemental::ReactionEngine engine;
    elemental::TargetState target;
    const auto hydro = actor("hydro");
    const auto electro = actor("electro", 200.0);

    (void)engine.apply(target, aura_attack(elemental::Element::hydro, 2.0, hydro));
    const auto initial =
        engine.apply(target, aura_attack(elemental::Element::electro, 1.0, electro));
    check(has_reaction(initial, elemental::Reaction::electro_charged), "Hydro + Electro reaction");
    check(target.is_electro_charged(), "Electro-Charged state should coexist after first tick");
    near(target.aura_gauge(elemental::Element::hydro), 1.2, 1.0e-9, "EC Hydro tick cost");
    near(target.aura_gauge(elemental::Element::electro), 0.4, 1.0e-9, "EC Electro tick cost");

    const auto ticks = engine.advance(target, 1.0);
    check(ticks.size() == 1, "Electro-Charged should tick once after one second");
    check(ticks.front().periodic, "Electro-Charged timer event should be periodic");
    near(ticks.front().time_seconds, 1.0, 1.0e-9, "periodic event timestamp");
    check(!target.is_electro_charged(), "EC ends when either coexistence aura is exhausted");
}

void quicken_and_aggravate() {
    elemental::ReactionEngine engine;
    elemental::TargetState target;
    const auto electro = actor("electro");
    const auto dendro = actor("dendro");

    (void)engine.apply(target, aura_attack(elemental::Element::electro, 2.0, electro));
    const auto quicken =
        engine.apply(target, aura_attack(elemental::Element::dendro, 1.0, dendro));
    check(has_reaction(quicken, elemental::Reaction::quicken), "Dendro + Electro should Quicken");
    check(target.is_quickened(), "Quicken status should exist");
    near(target.quicken_remaining_seconds(), 11.0, 1.0e-9, "1U Quicken duration");

    const auto aggravate =
        engine.apply(target, aura_attack(elemental::Element::electro, 1.0, electro));
    check(has_reaction(aggravate, elemental::Reaction::aggravate), "Electro on Quicken Aggravates");
    near(
        aggravate.additive_base_damage,
        1.15 * 1446.853458,
        1.0e-6,
        "Aggravate additive base damage");
    check(target.is_quickened(), "Aggravate must not consume Quicken");
}

void bloom_lifecycle() {
    elemental::ReactionEngine engine;
    elemental::TargetState target;
    const auto dendro = actor("dendro");
    const auto hydro = actor("hydro");

    elemental::Resolution sixth;
    for (int i = 0; i < 6; ++i) {
        (void)engine.apply(target, aura_attack(elemental::Element::dendro, 4.0, dendro));
        sixth = engine.apply(target, aura_attack(elemental::Element::hydro, 1.0, hydro));
    }
    check(target.dendro_core_count() == 5, "the field holds at most five Dendro Cores");
    check(
        has_reaction(sixth, elemental::Reaction::bloom_explosion),
        "creating a sixth core explodes the oldest");

    const auto hyperblooms =
        engine.trigger_dendro_cores(target, elemental::Element::electro, actor("electro", 800.0), 2);
    check(hyperblooms.size() == 2, "core trigger limit should be respected");
    check(
        hyperblooms.front().reaction == elemental::Reaction::hyperbloom,
        "Electro should trigger Hyperbloom");
    check(target.dendro_core_count() == 3, "triggered cores should be removed");

    const auto expired = engine.advance(target, 6.0);
    check(expired.size() == 3, "remaining Dendro Cores explode after six seconds");
    check(target.dendro_core_count() == 0, "expired cores should leave the field");
}

void burning_ticks_and_fuel() {
    elemental::ReactionEngine engine;
    elemental::TargetState target;
    const auto dendro = actor("dendro");
    const auto pyro = actor("pyro", 300.0);

    (void)engine.apply(target, aura_attack(elemental::Element::dendro, 1.0, dendro));
    const auto burning = engine.apply(target, aura_attack(elemental::Element::pyro, 1.0, pyro));
    check(has_reaction(burning, elemental::Reaction::burning), "Dendro + Pyro should Burn");
    check(target.is_burning(), "Burning state should exist");

    const auto ticks = engine.advance(target, 0.5);
    check(ticks.size() == 2, "Burning ticks every 0.25 seconds");
    near(
        target.aura_gauge(elemental::Element::dendro),
        0.6,
        1.0e-9,
        "Burning consumes at least 0.4U Dendro per second");
}

void frozen_shatter_and_superconduct() {
    elemental::ReactionEngine engine;
    const auto hydro = actor("hydro");
    const auto cryo = actor("cryo");
    const auto electro = actor("electro");

    elemental::TargetState frozen_target;
    (void)engine.apply(frozen_target, aura_attack(elemental::Element::hydro, 2.0, hydro));
    const auto frozen =
        engine.apply(frozen_target, aura_attack(elemental::Element::cryo, 1.0, cryo));
    check(has_reaction(frozen, elemental::Reaction::frozen), "Hydro + Cryo should Freeze");
    check(frozen_target.is_frozen(), "Frozen state should be active");

    auto heavy = aura_attack(elemental::Element::physical, 0.0, actor("claymore"));
    heavy.blunt = true;
    const auto shattered = engine.apply(frozen_target, heavy);
    check(has_reaction(shattered, elemental::Reaction::shatter), "blunt hit should Shatter");
    check(!frozen_target.is_frozen(), "Shatter removes Frozen");

    elemental::TargetStats stats;
    stats.set_resistance(elemental::Element::physical, 0.1);
    elemental::TargetState superconduct_target(stats);
    (void)engine.apply(superconduct_target, aura_attack(elemental::Element::cryo, 1.0, cryo));
    (void)engine.apply(
        superconduct_target, aura_attack(elemental::Element::electro, 1.0, electro));
    near(
        superconduct_target.superconduct_remaining_seconds(),
        12.0,
        1.0e-9,
        "Superconduct debuff duration");
    auto physical = aura_attack(elemental::Element::physical, 0.0, actor("physical"));
    physical.base_damage = 1000.0;
    const auto physical_hit = engine.apply(superconduct_target, physical);
    near(physical_hit.attack_damage, 575.0, 1.0e-6, "Superconduct applies 40% Physical RES shred");
}

void swirl_priority_and_crystallize_cooldown() {
    elemental::ReactionEngine engine;
    const auto hydro = actor("hydro");
    const auto electro = actor("electro");
    const auto anemo = actor("anemo");
    const auto geo = actor("geo");
    const auto pyro = actor("pyro");

    elemental::TargetState electro_charged;
    (void)engine.apply(
        electro_charged, aura_attack(elemental::Element::hydro, 4.0, hydro));
    (void)engine.apply(
        electro_charged, aura_attack(elemental::Element::electro, 2.0, electro));
    const auto swirl =
        engine.apply(electro_charged, aura_attack(elemental::Element::anemo, 1.0, anemo));
    check(swirl.events.size() == 1, "small Anemo application only Swirls Electro on EC");
    check(
        swirl.events.front().damage_element == elemental::Element::electro,
        "Electro has Swirl priority on Electro-Charged");

    elemental::TargetState crystallize_target;
    (void)engine.apply(
        crystallize_target, aura_attack(elemental::Element::pyro, 2.0, pyro));
    const auto first =
        engine.apply(crystallize_target, aura_attack(elemental::Element::geo, 1.0, geo));
    check(has_reaction(first, elemental::Reaction::crystallize), "Geo should Crystallize Pyro");
    const double after_first = crystallize_target.aura_gauge(elemental::Element::pyro);
    const auto blocked =
        engine.apply(crystallize_target, aura_attack(elemental::Element::geo, 1.0, geo));
    check(blocked.events.empty(), "Crystallize has a one-second global target cooldown");
    near(
        crystallize_target.aura_gauge(elemental::Element::pyro),
        after_first,
        1.0e-9,
        "blocked Crystallize does not consume aura");
    (void)engine.advance(crystallize_target, 1.0);
    const auto ready =
        engine.apply(crystallize_target, aura_attack(elemental::Element::geo, 1.0, geo));
    check(has_reaction(ready, elemental::Reaction::crystallize), "Crystallize cooldown expires");
}

void formulas_and_icd() {
    elemental::TargetStats target;
    target.set_resistance(elemental::Element::pyro, 0.1);
    const double overloaded = elemental::DamageModel::transformative_damage(
        elemental::Reaction::overloaded,
        elemental::Element::pyro,
        actor("pyro"),
        target);
    near(overloaded, 2.75 * 1446.853458 * 0.9, 1.0e-6, "Overloaded v5.2 multiplier");

    elemental::StandardIcdTracker icd;
    check(icd.can_apply("normal", 0.0), "first ICD hit applies");
    check(!icd.can_apply("normal", 0.1), "second ICD hit is blocked");
    check(!icd.can_apply("normal", 0.2), "third sequence hit is blocked");
    check(icd.can_apply("normal", 0.3), "fourth sequence hit satisfies the three-hit rule");
    check(icd.can_apply("normal", 2.8), "2.5-second rule applies independently");
}

void lunar_shared_formula() {
    elemental::TargetStats target;
    const elemental::LunarContributor contributor{actor("lunar"), 1.0};
    const double damage = elemental::DamageModel::lunar_indirect_damage(
        elemental::Reaction::lunar_charged, {contributor}, target);
    near(damage, 1.8 * 1446.853458, 1.0e-6, "single-contributor Lunar-Charged formula");
}

} // namespace

int main() {
    aura_tax_and_decay();
    vaporize_directions_and_consumption();
    electro_charged_coexistence_and_ticks();
    quicken_and_aggravate();
    bloom_lifecycle();
    burning_ticks_and_fuel();
    frozen_shatter_and_superconduct();
    swirl_priority_and_crystallize_cooldown();
    formulas_and_icd();
    lunar_shared_formula();

    if (failures == 0) {
        std::cout << "All elemental reaction tests passed.\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed.\n";
    return EXIT_FAILURE;
}
