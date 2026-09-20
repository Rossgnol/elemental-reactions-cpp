#include "elemental/reaction.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <utility>

namespace elemental {
namespace {

constexpr double epsilon = 1.0e-9;

constexpr std::size_t index_of(Element element) noexcept {
    return static_cast<std::size_t>(element);
}

constexpr std::size_t index_of(Reaction reaction) noexcept {
    return static_cast<std::size_t>(reaction);
}

constexpr bool is_storable_aura(Element element) noexcept {
    return element == Element::pyro || element == Element::hydro ||
           element == Element::electro || element == Element::cryo ||
           element == Element::dendro;
}

double decay_seconds_per_unit(double source_gauge, double aura_tax) {
    if (source_gauge <= 0.0 || aura_tax <= 0.0) {
        return 0.0;
    }
    return (2.5 * source_gauge + 7.0) / (aura_tax * source_gauge);
}

Element lunar_damage_element(Reaction reaction) {
    switch (reaction) {
    case Reaction::lunar_charged:
        return Element::electro;
    case Reaction::lunar_bloom:
        return Element::dendro;
    case Reaction::lunar_crystallize:
        return Element::geo;
    default:
        throw std::invalid_argument("reaction is not a Lunar reaction");
    }
}

double lunar_em_bonus(double elemental_mastery) noexcept {
    const double em = std::max(0.0, elemental_mastery);
    return 6.0 * em / (em + 2000.0);
}

} // namespace

std::string_view to_string(Element element) noexcept {
    constexpr std::array<std::string_view, element_count> names{
        "Physical", "Pyro", "Hydro", "Electro", "Cryo", "Anemo", "Geo", "Dendro"};
    const auto index = index_of(element);
    return index < names.size() ? names[index] : "Unknown";
}

std::string_view to_string(Reaction reaction) noexcept {
    constexpr std::array<std::string_view, reaction_count> names{
        "None", "Vaporize", "Melt", "Overloaded", "Electro-Charged",
        "Superconduct", "Frozen", "Shatter", "Swirl", "Crystallize",
        "Burning", "Bloom", "Bloom Explosion", "Hyperbloom", "Burgeon",
        "Quicken", "Aggravate", "Spread", "Lunar-Charged", "Lunar-Bloom",
        "Lunar-Crystallize"};
    const auto index = index_of(reaction);
    return index < names.size() ? names[index] : "Unknown";
}

double ActorStats::bonus(Reaction reaction) const noexcept {
    const auto index = index_of(reaction);
    return index < reaction_bonus.size() ? reaction_bonus[index] : 0.0;
}

void ActorStats::set_bonus(Reaction reaction, double value) noexcept {
    const auto index = index_of(reaction);
    if (index < reaction_bonus.size()) {
        reaction_bonus[index] = value;
    }
}

double TargetStats::resistance_to(Element element) const noexcept {
    const auto index = index_of(element);
    return index < resistance.size() ? resistance[index] : 0.0;
}

void TargetStats::set_resistance(Element element, double value) noexcept {
    const auto index = index_of(element);
    if (index < resistance.size()) {
        resistance[index] = value;
    }
}

double DamageModel::player_level_multiplier(int level) {
    // Character reaction level curve. Levels 1-90 are the fully published
    // character values. Level 95 and 100 are included because those two
    // post-90 values are also published; intermediate post-90 values are not.
    static constexpr std::array<double, 91> values{
        0.0,
        17.165605, 18.535048, 19.904854, 21.274903, 22.645400,
        24.649613, 26.640643, 28.868587, 31.367679, 34.143343,
        37.201000, 40.660000, 44.446668, 48.563519, 53.748480,
        59.081897, 64.420047, 69.724455, 75.123137, 80.584775,
        86.112028, 91.703742, 97.244628, 102.812644, 108.409563,
        113.201694, 118.102906, 122.979318, 129.727330, 136.292910,
        142.670850, 149.029029, 155.416987, 161.825495, 169.106313,
        176.518077, 184.072741, 191.709518, 199.556908, 207.382042,
        215.398900, 224.165667, 233.502160, 243.350573, 256.063067,
        268.543493, 281.526075, 295.013648, 309.067188, 323.601597,
        336.757542, 350.530312, 364.482705, 378.619181, 398.600417,
        416.398254, 434.386996, 452.951051, 472.606217, 492.884890,
        513.568543, 539.103198, 565.510563, 592.538753, 624.443427,
        651.470148, 679.496830, 707.794060, 736.671422, 765.640231,
        794.773403, 824.677397, 851.157781, 877.742090, 914.229123,
        946.746752, 979.411386, 1011.223022, 1044.791746, 1077.443668,
        1109.997540, 1142.976615, 1176.369483, 1210.184393, 1253.835659,
        1288.952801, 1325.484092, 1363.456928, 1405.097377, 1446.853458};

    if (level >= 1 && level <= 90) {
        return values[static_cast<std::size_t>(level)];
    }
    if (level == 95) {
        return 1561.468;
    }
    if (level == 100) {
        return 1674.8092;
    }
    throw std::out_of_range("supported character reaction levels are 1-90, 95, and 100");
}

double DamageModel::crystallize_level_multiplier(int level) {
    static constexpr std::array<double, 91> values{
        0.0,
        91.179100, 98.707667, 106.236220, 113.764771, 121.293322,
        128.821878, 136.350422, 143.878978, 151.407522, 158.936078,
        169.991484, 181.076253, 192.190362, 204.048207, 215.938996,
        227.862750, 247.685944, 267.542105, 287.431209, 303.826417,
        320.225217, 336.627633, 352.319267, 368.010913, 383.702548,
        394.432358, 405.181470, 415.949907, 426.737645, 437.544709,
        450.600004, 463.700301, 476.845577, 491.127512, 502.554564,
        514.012104, 531.409589, 549.979601, 568.584880, 584.996520,
        605.670375, 626.386206, 646.052333, 665.755638, 685.496096,
        700.839402, 723.333147, 745.865265, 768.435731, 786.791945,
        809.538812, 832.329057, 855.162654, 878.039628, 899.484802,
        919.362018, 946.039586, 974.764223, 1003.578617, 1030.077002,
        1056.634974, 1085.246306, 1113.924427, 1149.258720, 1178.064819,
        1200.223743, 1227.660294, 1257.242987, 1284.917392, 1314.752880,
        1342.665216, 1372.752485, 1396.320986, 1427.312436, 1458.374528,
        1482.335772, 1511.910837, 1541.549377, 1569.153701, 1596.814298,
        1622.419626, 1648.074031, 1666.376146, 1684.678276, 1702.980391,
        1726.104684, 1754.671567, 1785.866560, 1817.137404, 1851.060358};

    if (level >= 1 && level <= 90) {
        return values[static_cast<std::size_t>(level)];
    }
    if (level == 95) {
        return 2041.569;
    }
    if (level == 100) {
        return 2198.814;
    }
    throw std::out_of_range("supported Crystallize levels are 1-90, 95, and 100");
}

double DamageModel::resistance_multiplier(double resistance) noexcept {
    if (resistance < 0.0) {
        return 1.0 - resistance / 2.0;
    }
    if (resistance < 0.75) {
        return 1.0 - resistance;
    }
    return 1.0 / (4.0 * resistance + 1.0);
}

double DamageModel::defense_multiplier(
    int attacker_level,
    int target_level,
    double defense_reduction,
    double defense_ignore) noexcept {
    const double attacker = static_cast<double>(std::max(1, attacker_level)) + 100.0;
    const double target = static_cast<double>(std::max(1, target_level)) + 100.0;
    const double reduction = std::clamp(defense_reduction, 0.0, 1.0);
    const double ignore = std::clamp(defense_ignore, 0.0, 1.0);
    return attacker / (attacker + target * (1.0 - reduction) * (1.0 - ignore));
}

double DamageModel::amplifying_multiplier(
    Reaction reaction,
    Element trigger,
    const ActorStats& actor) {
    double base = 0.0;
    if (reaction == Reaction::vaporize) {
        base = trigger == Element::hydro ? 2.0 : (trigger == Element::pyro ? 1.5 : 0.0);
    } else if (reaction == Reaction::melt) {
        base = trigger == Element::pyro ? 2.0 : (trigger == Element::cryo ? 1.5 : 0.0);
    }
    if (base == 0.0) {
        throw std::invalid_argument("invalid trigger for amplifying reaction");
    }
    const double em = std::max(0.0, actor.elemental_mastery);
    return base * (1.0 + 2.78 * em / (em + 1400.0) + actor.bonus(reaction));
}

double DamageModel::additive_base_damage(Reaction reaction, const ActorStats& actor) {
    const double multiplier = reaction == Reaction::aggravate
                                  ? 1.15
                                  : (reaction == Reaction::spread ? 1.25 : 0.0);
    if (multiplier == 0.0) {
        throw std::invalid_argument("reaction is not additive");
    }
    const double em = std::max(0.0, actor.elemental_mastery);
    return multiplier * player_level_multiplier(actor.level) *
           (1.0 + 5.0 * em / (em + 1200.0) + actor.bonus(reaction));
}

double DamageModel::transformative_damage(
    Reaction reaction,
    Element damage_element,
    const ActorStats& actor,
    const TargetStats& target) {
    double multiplier = 0.0;
    switch (reaction) {
    case Reaction::burning:
        multiplier = 0.25;
        break;
    case Reaction::swirl:
        multiplier = 0.6;
        break;
    case Reaction::superconduct:
        multiplier = 1.5;
        break;
    case Reaction::electro_charged:
    case Reaction::bloom:
    case Reaction::bloom_explosion:
        multiplier = 2.0;
        break;
    case Reaction::overloaded:
        multiplier = 2.75;
        break;
    case Reaction::shatter:
    case Reaction::hyperbloom:
    case Reaction::burgeon:
        multiplier = 3.0;
        break;
    default:
        throw std::invalid_argument("reaction does not deal transformative damage");
    }

    const double em = std::max(0.0, actor.elemental_mastery);
    const double mastery = 16.0 * em / (em + 2000.0);
    const double resistance = target.resistance_to(damage_element);
    return multiplier * player_level_multiplier(actor.level) *
           (1.0 + mastery + actor.bonus(reaction)) * resistance_multiplier(resistance);
}

double DamageModel::crystallize_shield(const ActorStats& actor) {
    const double em = std::max(0.0, actor.elemental_mastery);
    return crystallize_level_multiplier(actor.level) *
           (1.0 + 4.44 * em / (em + 1400.0) + actor.bonus(Reaction::crystallize));
}

double DamageModel::attack_damage(
    const Attack& attack,
    const TargetStats& target,
    double additive,
    double amplifying) {
    if (attack.base_damage <= 0.0) {
        return 0.0;
    }
    const double resistance = target.resistance_to(attack.element);
    return (attack.base_damage + additive) * (1.0 + attack.damage_bonus) *
           std::max(0.0, attack.critical_multiplier) *
           defense_multiplier(
               attack.source.level,
               target.level,
               target.defense_reduction,
               attack.defense_ignore) *
           resistance_multiplier(resistance) * amplifying;
}

double DamageModel::lunar_indirect_damage(
    Reaction reaction,
    const std::vector<LunarContributor>& contributors,
    const TargetStats& target,
    double lunar_base_damage_bonus,
    double elevation_multiplier) {
    if (reaction != Reaction::lunar_charged && reaction != Reaction::lunar_crystallize) {
        throw std::invalid_argument("only Lunar-Charged and Lunar-Crystallize have indirect damage");
    }
    const double reaction_multiplier = reaction == Reaction::lunar_charged ? 1.8 : 0.96;
    const Element element = lunar_damage_element(reaction);
    std::vector<double> individual;
    individual.reserve(contributors.size());
    for (const auto& contributor : contributors) {
        const double value = reaction_multiplier *
                             player_level_multiplier(contributor.stats.level) *
                             (1.0 + lunar_base_damage_bonus) *
                             (1.0 + lunar_em_bonus(contributor.stats.elemental_mastery) +
                              contributor.stats.bonus(reaction)) *
                             std::max(0.0, elevation_multiplier) *
                             resistance_multiplier(target.resistance_to(element)) *
                             std::max(0.0, contributor.critical_multiplier);
        individual.push_back(value);
    }
    std::sort(individual.begin(), individual.end(), std::greater<double>{});
    constexpr std::array<double, 4> weights{1.0, 0.5, 1.0 / 12.0, 1.0 / 12.0};
    double result = 0.0;
    const std::size_t count = std::min(individual.size(), weights.size());
    for (std::size_t i = 0; i < count; ++i) {
        result += individual[i] * weights[i];
    }
    return result;
}

double DamageModel::lunar_direct_damage(
    Reaction reaction,
    double scaling_stat,
    double talent_multiplier,
    const LunarContributor& contributor,
    const TargetStats& target,
    double lunar_base_damage_bonus,
    double elevation_multiplier) {
    double reaction_multiplier = 0.0;
    switch (reaction) {
    case Reaction::lunar_charged:
        reaction_multiplier = 3.0;
        break;
    case Reaction::lunar_bloom:
        reaction_multiplier = 1.0;
        break;
    case Reaction::lunar_crystallize:
        reaction_multiplier = 1.6;
        break;
    default:
        throw std::invalid_argument("reaction is not a Lunar reaction");
    }
    const Element element = lunar_damage_element(reaction);
    return std::max(0.0, scaling_stat) * std::max(0.0, talent_multiplier) *
           reaction_multiplier * (1.0 + lunar_base_damage_bonus) *
           (1.0 + lunar_em_bonus(contributor.stats.elemental_mastery) +
            contributor.stats.bonus(reaction)) *
           std::max(0.0, elevation_multiplier) *
           resistance_multiplier(target.resistance_to(element)) *
           std::max(0.0, contributor.critical_multiplier);
}

TargetState::TargetState(TargetStats stats) : stats_(std::move(stats)) {}

TargetStats& TargetState::stats() noexcept {
    return stats_;
}

const TargetStats& TargetState::stats() const noexcept {
    return stats_;
}

double TargetState::time_seconds() const noexcept {
    return time_seconds_;
}

double TargetState::aura_gauge(Element element) const noexcept {
    const auto index = index_of(element);
    if (index >= auras_.size() || !auras_[index].active) {
        return 0.0;
    }
    return auras_[index].gauge;
}

double TargetState::aura_remaining_seconds(Element element) const noexcept {
    const auto index = index_of(element);
    if (index >= auras_.size() || !auras_[index].active) {
        return 0.0;
    }
    if (element == Element::dendro && burning_.active) {
        const auto& aura = auras_[index];
        const double cost = std::max(0.4, 2.0 / aura.decay_seconds_per_unit);
        return cost > 0.0 ? aura.gauge / cost : 0.0;
    }
    return auras_[index].gauge * auras_[index].decay_seconds_per_unit;
}

bool TargetState::is_frozen() const noexcept {
    return frozen_.active;
}

bool TargetState::is_burning() const noexcept {
    return burning_.active;
}

bool TargetState::is_electro_charged() const noexcept {
    return electro_charged_.active;
}

bool TargetState::is_quickened() const noexcept {
    return quicken_.active;
}

double TargetState::quicken_remaining_seconds() const noexcept {
    return quicken_.active ? quicken_.gauge * quicken_.decay_seconds_per_unit : 0.0;
}

double TargetState::superconduct_remaining_seconds() const noexcept {
    return superconduct_remaining_seconds_;
}

std::size_t TargetState::dendro_core_count() const noexcept {
    return cores_.size();
}

ReactionEngine::ReactionEngine(EngineConfig config) : config_(std::move(config)) {
    if (config_.aura_tax <= 0.0 || config_.aura_tax > 1.0 ||
        config_.electro_charged_tick_seconds <= 0.0 ||
        config_.burning_tick_seconds <= 0.0 ||
        config_.dendro_core_lifetime_seconds <= 0.0 ||
        config_.maximum_dendro_cores == 0) {
        throw std::invalid_argument("invalid reaction engine configuration");
    }
}

const EngineConfig& ReactionEngine::config() const noexcept {
    return config_;
}

Resolution ReactionEngine::apply(TargetState& target, const Attack& attack) const {
    if (attack.gauge_units < 0.0) {
        throw std::invalid_argument("gauge_units cannot be negative");
    }

    Resolution result;
    double trigger_gauge = attack.applies_aura ? attack.gauge_units : 0.0;
    bool reacted = false;
    bool trigger_consumed = false;
    bool trigger_was_stored = false;
    bool frozen_aura_processed = false;

    auto& auras = target.auras_;
    auto compute_attack_damage = [&]() {
        TargetStats effective_target = target.stats_;
        if (target.superconduct_remaining_seconds_ > epsilon) {
            const double physical = effective_target.resistance_to(Element::physical);
            effective_target.set_resistance(Element::physical, physical - 0.4);
        }
        return DamageModel::attack_damage(
            attack, effective_target, result.additive_base_damage, result.attack_multiplier);
    };
    auto aura_active = [&](Element element) {
        return auras[index_of(element)].active && auras[index_of(element)].gauge > epsilon;
    };
    auto clear_aura_if_empty = [&](Element element) {
        auto& aura = auras[index_of(element)];
        if (aura.gauge <= epsilon) {
            aura = {};
        }
    };
    auto consume_aura = [&](Element element, double requested) {
        auto& aura = auras[index_of(element)];
        if (!aura.active || requested <= 0.0) {
            return 0.0;
        }
        const double consumed = std::min(aura.gauge, requested);
        aura.gauge -= consumed;
        clear_aura_if_empty(element);
        return consumed;
    };
    auto store_aura = [&](Element element, double source_gauge, const ActorStats& owner, bool overwrite) {
        if (!is_storable_aura(element) || source_gauge <= 0.0) {
            return;
        }
        auto& aura = auras[index_of(element)];
        const double taxed = source_gauge * config_.aura_tax;
        const double new_decay = decay_seconds_per_unit(source_gauge, config_.aura_tax);
        if (!aura.active || overwrite) {
            aura = {true, taxed, new_decay, source_gauge, owner};
            return;
        }
        if (taxed > aura.gauge + epsilon) {
            aura.gauge = taxed;
            aura.source_gauge_units = source_gauge;
            // Since 3.0, Pyro adopts the new decay rate when its gauge changes.
            if (element == Element::pyro) {
                aura.decay_seconds_per_unit = new_decay;
            }
        }
        aura.owner = owner;
    };
    auto emit = [&](Reaction reaction,
                    Element damage_element,
                    const ActorStats& actor,
                    double gauge_consumed,
                    std::string note = {}) -> ReactionEvent& {
        ReactionEvent event;
        event.time_seconds = target.time_seconds_;
        event.reaction = reaction;
        event.damage_element = damage_element;
        event.source_id = actor.id;
        event.gauge_consumed = gauge_consumed;
        event.note = std::move(note);

        if (reaction == Reaction::vaporize || reaction == Reaction::melt) {
            event.attack_multiplier =
                DamageModel::amplifying_multiplier(reaction, attack.element, actor);
            result.attack_multiplier = event.attack_multiplier;
        } else if (reaction == Reaction::aggravate || reaction == Reaction::spread) {
            event.additive_base_damage = DamageModel::additive_base_damage(reaction, actor);
            result.additive_base_damage += event.additive_base_damage;
        } else if (reaction == Reaction::crystallize) {
            event.shield_strength = DamageModel::crystallize_shield(actor);
            event.duration_seconds = 15.0;
        } else if (reaction == Reaction::overloaded || reaction == Reaction::electro_charged ||
                   reaction == Reaction::superconduct || reaction == Reaction::shatter ||
                   reaction == Reaction::swirl || reaction == Reaction::burning ||
                   reaction == Reaction::bloom_explosion || reaction == Reaction::hyperbloom ||
                   reaction == Reaction::burgeon) {
            event.independent_damage =
                DamageModel::transformative_damage(reaction, damage_element, actor, target.stats_);
        }
        result.events.push_back(std::move(event));
        reacted = true;
        return result.events.back();
    };
    auto explode_oldest_core = [&]() {
        if (target.cores_.empty()) {
            return;
        }
        const auto core = target.cores_.front();
        target.cores_.erase(target.cores_.begin());
        auto& event = emit(
            Reaction::bloom_explosion,
            Element::dendro,
            core.owner,
            0.0,
            "oldest core exploded because the field limit was exceeded");
        event.core_id = core.id;
    };
    auto create_core = [&](double gauge_consumed) {
        if (target.cores_.size() >= config_.maximum_dendro_cores) {
            explode_oldest_core();
        }
        const int id = target.next_core_id_++;
        target.cores_.push_back(
            {id, target.time_seconds_ + config_.dendro_core_lifetime_seconds, attack.source});
        ReactionEvent event;
        event.time_seconds = target.time_seconds_;
        event.reaction = Reaction::bloom;
        event.damage_element = Element::dendro;
        event.source_id = attack.source.id;
        event.gauge_consumed = gauge_consumed;
        event.core_id = id;
        event.duration_seconds = config_.dendro_core_lifetime_seconds;
        event.note = "Dendro Core created";
        result.events.push_back(std::move(event));
        reacted = true;
    };

    // Geo attacks are normally blunt. A blunt hit Shatters before its Elemental
    // component is resolved, exposing any underlying aura.
    if (target.frozen_.active && (attack.blunt || attack.element == Element::geo)) {
        emit(Reaction::shatter, Element::physical, attack.source, target.frozen_.gauge);
        target.frozen_ = {};
        frozen_aura_processed = true;
    }

    // Burning acts as a 2U, non-decaying Pyro-like aura above its underlying
    // Pyro and Dendro auras. Strong enough triggers may pass through it.
    if (target.burning_.active && attack.element != Element::pyro &&
        attack.element != Element::dendro && trigger_gauge > 0.0) {
        double coefficient = 0.0;
        Reaction reaction = Reaction::none;
        Element damage_element = Element::physical;
        if (attack.element == Element::hydro) {
            coefficient = 2.0;
            reaction = Reaction::vaporize;
            damage_element = Element::hydro;
        } else if (attack.element == Element::cryo) {
            coefficient = 0.5;
            reaction = Reaction::melt;
            damage_element = Element::cryo;
        } else if (attack.element == Element::electro) {
            coefficient = 1.0;
            reaction = Reaction::overloaded;
            damage_element = Element::pyro;
        } else if (attack.element == Element::anemo) {
            coefficient = 0.5;
            reaction = Reaction::swirl;
            damage_element = Element::pyro;
        } else if (attack.element == Element::geo) {
            coefficient = 0.5;
            reaction = Reaction::crystallize;
            damage_element = Element::pyro;
        }

        if (coefficient > 0.0) {
            const double burning_before = target.burning_.gauge;
            const double consumed = std::min(burning_before, coefficient * trigger_gauge);
            target.burning_.gauge -= consumed;
            consume_aura(Element::pyro, consumed);
            emit(reaction, damage_element, attack.source, consumed, "reaction with Burning aura");
            const double trigger_used = consumed / coefficient;
            trigger_gauge = std::max(0.0, trigger_gauge - trigger_used);
            trigger_consumed = true;
            if (target.burning_.gauge <= epsilon || !aura_active(Element::dendro)) {
                target.burning_ = {};
            } else {
                result.attack_damage = compute_attack_damage();
                return result;
            }
        }
    }

    // With Frozen + underlying Hydro, a small Anemo application Swirls only
    // Hydro. It reaches Frozen too only when its reduction exceeds Hydro gauge.
    if (target.frozen_.active && attack.element == Element::anemo &&
        aura_active(Element::hydro) && 0.5 * trigger_gauge <=
                                           auras[index_of(Element::hydro)].gauge + epsilon) {
        frozen_aura_processed = true;
    }

    // Frozen is a Cryo-like aura with its own gauge. Non-blunt reactions stop
    // at this layer; a strong Anemo trigger may continue to underlying Hydro.
    if (target.frozen_.active && !frozen_aura_processed && trigger_gauge > 0.0) {
        double coefficient = 0.0;
        Reaction reaction = Reaction::none;
        Element damage_element = Element::physical;
        if (attack.element == Element::pyro) {
            coefficient = 2.0;
            reaction = Reaction::melt;
            damage_element = Element::pyro;
        } else if (attack.element == Element::electro) {
            coefficient = 1.0;
            reaction = Reaction::superconduct;
            damage_element = Element::cryo;
        } else if (attack.element == Element::anemo) {
            coefficient = 0.5;
            reaction = Reaction::swirl;
            damage_element = Element::cryo;
        }
        if (coefficient > 0.0) {
            const double consumed = std::min(target.frozen_.gauge, coefficient * trigger_gauge);
            target.frozen_.gauge -= consumed;
            if (target.frozen_.gauge <= epsilon) {
                target.frozen_ = {};
            }
            emit(reaction, damage_element, attack.source, consumed, "reaction with Frozen aura");
            trigger_consumed = true;
            frozen_aura_processed = true;
            if (reaction == Reaction::superconduct) {
                target.superconduct_remaining_seconds_ = 12.0;
                result.events.back().duration_seconds = 12.0;
            }
            if (attack.element != Element::anemo) {
                result.attack_damage = compute_attack_damage();
                return result;
            }
        }
    }

    auto react_with_aura = [&](Element aura_element,
                               Reaction reaction,
                               double coefficient,
                               Element damage_element) {
        if (!aura_active(aura_element) || trigger_gauge <= 0.0) {
            return false;
        }
        const double consumed = consume_aura(aura_element, coefficient * trigger_gauge);
        auto& event = emit(reaction, damage_element, attack.source, consumed);
        trigger_consumed = true;
        if (reaction == Reaction::superconduct) {
            target.superconduct_remaining_seconds_ = 12.0;
            event.duration_seconds = 12.0;
        }
        return true;
    };
    auto start_frozen = [&](Element origin) {
        if (!aura_active(origin) || trigger_gauge <= 0.0) {
            return false;
        }
        const double origin_gauge = auras[index_of(origin)].gauge;
        const double frozen_gauge = 2.0 * std::min(origin_gauge, trigger_gauge);
        const double consumed = consume_aura(origin, trigger_gauge);
        if (!target.frozen_.active || frozen_gauge > target.frozen_.gauge) {
            target.frozen_ = {true, frozen_gauge, 0.0};
        }
        const double resistance = std::clamp(target.stats_.freeze_resistance, 0.0, 0.999999);
        const double duration =
            2.0 * std::sqrt(5.0 * target.frozen_.gauge * (1.0 - resistance) + 4.0) - 4.0;
        auto& event = emit(Reaction::frozen, Element::cryo, attack.source, consumed);
        event.duration_seconds = duration;
        target.frozen_.active = true;
        trigger_consumed = true;
        return true;
    };
    auto start_quicken = [&](Element origin) {
        if (!aura_active(origin) || trigger_gauge <= 0.0) {
            return false;
        }
        const double origin_gauge = auras[index_of(origin)].gauge;
        const double quicken_gauge = std::min(origin_gauge, trigger_gauge);
        const double consumed = consume_aura(origin, trigger_gauge);
        if (!target.quicken_.active || quicken_gauge > target.quicken_.gauge) {
            target.quicken_.active = true;
            target.quicken_.gauge = quicken_gauge;
            target.quicken_.decay_seconds_per_unit = (5.0 * quicken_gauge + 6.0) / quicken_gauge;
            target.quicken_.owner = attack.source;
        }
        auto& event = emit(Reaction::quicken, Element::dendro, attack.source, consumed);
        event.duration_seconds = target.quicken_.gauge * target.quicken_.decay_seconds_per_unit;
        trigger_consumed = true;
        return true;
    };
    auto start_burning = [&](Element origin) {
        if (!aura_active(origin) || trigger_gauge <= 0.0 || target.burning_.active) {
            return false;
        }
        target.burning_.active = true;
        target.burning_.gauge = 2.0;
        target.burning_.next_tick = target.time_seconds_ + config_.burning_tick_seconds;
        target.burning_.next_pyro_application = target.time_seconds_ + 0.25;
        target.burning_.owner = attack.source;
        store_aura(attack.element, trigger_gauge, attack.source, attack.element == Element::dendro);
        trigger_was_stored = true;
        auto& event = emit(Reaction::burning, Element::pyro, attack.source, 0.0);
        event.periodic = false;
        return true;
    };
    auto start_bloom = [&](Element origin, double coefficient) {
        if (!aura_active(origin) || trigger_gauge <= 0.0) {
            return false;
        }
        const double consumed = consume_aura(origin, coefficient * trigger_gauge);
        create_core(consumed);
        trigger_consumed = true;
        return true;
    };
    auto start_electro_charged = [&](Element origin) {
        if (!aura_active(origin) || trigger_gauge <= 0.0) {
            return false;
        }
        store_aura(attack.element, trigger_gauge, attack.source, false);
        trigger_was_stored = true;
        target.electro_charged_.active = true;
        target.electro_charged_.owner = attack.source;
        auto& event = emit(
            Reaction::electro_charged,
            Element::electro,
            attack.source,
            0.0);
        if (target.time_seconds_ - target.electro_charged_.last_damage_time < 0.5 - epsilon) {
            event.independent_damage = 0.0;
            event.damage_suppressed = true;
            event.note = "Electro-Charged damage ICD";
        } else {
            event.gauge_consumed =
                consume_aura(Element::hydro, config_.electro_charged_gauge_cost) +
                consume_aura(Element::electro, config_.electro_charged_gauge_cost);
            target.electro_charged_.last_damage_time = target.time_seconds_;
        }
        target.electro_charged_.next_tick =
            target.time_seconds_ + config_.electro_charged_tick_seconds;
        if (!aura_active(Element::hydro) || !aura_active(Element::electro)) {
            target.electro_charged_.active = false;
        }
        return true;
    };

    // Applying Pyro/Dendro while already Burning refreshes ownership. Dendro
    // specifically overwrites its current underlying gauge.
    if (target.burning_.active &&
        (attack.element == Element::pyro || attack.element == Element::dendro) &&
        trigger_gauge > 0.0) {
        target.burning_.owner = attack.source;
        store_aura(
            attack.element,
            trigger_gauge,
            attack.source,
            attack.element == Element::dendro);
        trigger_was_stored = true;
    }

    switch (attack.element) {
    case Element::pyro:
        react_with_aura(Element::hydro, Reaction::vaporize, 0.5, Element::pyro);
        react_with_aura(Element::cryo, Reaction::melt, 2.0, Element::pyro);
        react_with_aura(Element::electro, Reaction::overloaded, 1.0, Element::pyro);
        if (!target.burning_.active) {
            if (!start_burning(Element::dendro) && target.quicken_.active) {
                const double converted = target.quicken_.gauge;
                target.quicken_ = {};
                auto& dendro = auras[index_of(Element::dendro)];
                dendro = {true,
                          converted,
                          decay_seconds_per_unit(1.0, config_.aura_tax),
                          1.0,
                          attack.source};
                start_burning(Element::dendro);
            }
        }
        break;
    case Element::hydro:
        react_with_aura(Element::pyro, Reaction::vaporize, 2.0, Element::hydro);
        start_frozen(Element::cryo);
        if (aura_active(Element::electro)) {
            start_electro_charged(Element::electro);
        }
        if (!start_bloom(Element::dendro, 0.5) && target.quicken_.active) {
            const double consumed = std::min(target.quicken_.gauge, 0.5 * trigger_gauge);
            target.quicken_.gauge -= consumed;
            if (target.quicken_.gauge <= epsilon) {
                target.quicken_ = {};
            }
            create_core(consumed);
            trigger_consumed = true;
        }
        break;
    case Element::electro:
        if (target.quicken_.active) {
            emit(Reaction::aggravate, Element::electro, attack.source, 0.0);
        }
        react_with_aura(Element::pyro, Reaction::overloaded, 1.0, Element::pyro);
        react_with_aura(Element::cryo, Reaction::superconduct, 1.0, Element::cryo);
        if (aura_active(Element::hydro)) {
            start_electro_charged(Element::hydro);
        }
        start_quicken(Element::dendro);
        break;
    case Element::cryo:
        react_with_aura(Element::pyro, Reaction::melt, 0.5, Element::cryo);
        start_frozen(Element::hydro);
        react_with_aura(Element::electro, Reaction::superconduct, 1.0, Element::cryo);
        break;
    case Element::dendro:
        if (target.quicken_.active) {
            emit(Reaction::spread, Element::dendro, attack.source, 0.0);
        }
        if (!target.burning_.active) {
            start_burning(Element::pyro);
        }
        start_bloom(Element::hydro, 2.0);
        start_quicken(Element::electro);
        break;
    case Element::anemo: {
        // Electro-Charged normally Swirls Electro only. When Anemo gauge
        // reduction exceeds the Electro aura it can also reach Hydro.
        if (target.electro_charged_.active && aura_active(Element::hydro) &&
            aura_active(Element::electro)) {
            const bool reaches_hydro =
                0.5 * trigger_gauge > auras[index_of(Element::electro)].gauge + epsilon;
            react_with_aura(Element::electro, Reaction::swirl, 0.5, Element::electro);
            if (reaches_hydro) {
                react_with_aura(Element::hydro, Reaction::swirl, 0.5, Element::hydro);
            }
        }
        constexpr std::array<Element, 4> priority{
            Element::hydro, Element::pyro, Element::cryo, Element::electro};
        for (const Element aura : priority) {
            if (target.electro_charged_.active &&
                (aura == Element::hydro || aura == Element::electro)) {
                continue;
            }
            if (aura == Element::cryo && frozen_aura_processed) {
                continue;
            }
            react_with_aura(aura, Reaction::swirl, 0.5, aura);
        }
        break;
    }
    case Element::geo: {
        if (target.time_seconds_ < target.crystallize_cooldown_until_ - epsilon) {
            break;
        }
        constexpr std::array<Element, 4> priority{
            Element::electro, Element::hydro, Element::pyro, Element::cryo};
        for (const Element aura : priority) {
            if (react_with_aura(aura, Reaction::crystallize, 0.5, aura)) {
                target.crystallize_cooldown_until_ = target.time_seconds_ + 1.0;
                break; // one shard per application
            }
        }
        break;
    }
    case Element::physical:
    case Element::count:
        break;
    }

    // Aggravate/Spread do not consume Quicken, and EC/Burning preserve the
    // second aura. All other trigger Elements disappear after reacting.
    if (attack.applies_aura && is_storable_aura(attack.element) && !trigger_was_stored &&
        (!reacted || !trigger_consumed)) {
        store_aura(attack.element, attack.gauge_units, attack.source, false);
    }

    result.attack_damage = compute_attack_damage();
    return result;
}

std::vector<ReactionEvent> ReactionEngine::advance(TargetState& target, double seconds) const {
    if (seconds < 0.0) {
        throw std::invalid_argument("advance duration cannot be negative");
    }
    std::vector<ReactionEvent> events;
    const double end_time = target.time_seconds_ + seconds;

    auto aura_active = [&](Element element) {
        const auto& aura = target.auras_[index_of(element)];
        return aura.active && aura.gauge > epsilon;
    };
    auto clear_empty = [&]() {
        for (auto& aura : target.auras_) {
            if (aura.active && aura.gauge <= epsilon) {
                aura = {};
            }
        }
        if (target.quicken_.active && target.quicken_.gauge <= epsilon) {
            target.quicken_ = {};
        }
        if (target.frozen_.active && target.frozen_.gauge <= epsilon) {
            target.frozen_ = {};
        }
        if (target.burning_.active &&
            (target.burning_.gauge <= epsilon || !aura_active(Element::dendro))) {
            target.burning_ = {};
        }
        if (target.electro_charged_.active &&
            (!aura_active(Element::hydro) || !aura_active(Element::electro))) {
            target.electro_charged_ = {};
        }
    };
    auto emit_periodic = [&](Reaction reaction,
                             Element element,
                             const ActorStats& actor,
                             bool periodic,
                             int core_id = 0) {
        ReactionEvent event;
        event.time_seconds = target.time_seconds_;
        event.reaction = reaction;
        event.damage_element = element;
        event.source_id = actor.id;
        event.independent_damage =
            DamageModel::transformative_damage(reaction, element, actor, target.stats_);
        event.periodic = periodic;
        event.core_id = core_id;
        events.push_back(std::move(event));
    };

    while (target.time_seconds_ < end_time - epsilon) {
        double next_time = end_time;
        if (target.burning_.active) {
            next_time = std::min(next_time, target.burning_.next_tick);
            next_time = std::min(next_time, target.burning_.next_pyro_application);
        }
        if (target.electro_charged_.active) {
            next_time = std::min(next_time, target.electro_charged_.next_tick);
        }
        for (const auto& core : target.cores_) {
            next_time = std::min(next_time, core.expires_at);
        }
        if (next_time < target.time_seconds_ + epsilon) {
            next_time = target.time_seconds_;
        }
        const double delta = next_time - target.time_seconds_;

        for (std::size_t i = 0; i < target.auras_.size(); ++i) {
            auto& aura = target.auras_[i];
            if (!aura.active || aura.decay_seconds_per_unit <= 0.0) {
                continue;
            }
            const Element element = static_cast<Element>(i);
            if (element == Element::dendro && target.burning_.active) {
                const double natural_rate = 1.0 / aura.decay_seconds_per_unit;
                const double special_rate = std::max(
                    config_.burning_dendro_minimum_cost_per_second, 2.0 * natural_rate);
                aura.gauge -= delta * special_rate;
            } else {
                aura.gauge -= delta / aura.decay_seconds_per_unit;
            }
        }
        if (target.quicken_.active && target.quicken_.decay_seconds_per_unit > 0.0) {
            target.quicken_.gauge -= delta / target.quicken_.decay_seconds_per_unit;
        }
        if (target.frozen_.active) {
            const double before = target.frozen_.elapsed_seconds;
            const double after = before + delta;
            const double kinematic_cost =
                0.4 * delta + 0.05 * (after * after - before * before);
            const double resistance =
                std::clamp(target.stats_.freeze_resistance, 0.0, 0.999999);
            target.frozen_.gauge -= kinematic_cost / (1.0 - resistance);
            target.frozen_.elapsed_seconds = after;
        }
        target.superconduct_remaining_seconds_ =
            std::max(0.0, target.superconduct_remaining_seconds_ - delta);
        target.time_seconds_ = next_time;
        clear_empty();

        for (std::size_t i = 0; i < target.cores_.size();) {
            if (target.cores_[i].expires_at <= target.time_seconds_ + epsilon) {
                const auto core = target.cores_[i];
                target.cores_.erase(target.cores_.begin() + static_cast<std::ptrdiff_t>(i));
                emit_periodic(
                    Reaction::bloom_explosion, Element::dendro, core.owner, true, core.id);
            } else {
                ++i;
            }
        }

        if (target.burning_.active &&
            target.burning_.next_pyro_application <= target.time_seconds_ + epsilon) {
            auto& pyro = target.auras_[index_of(Element::pyro)];
            const double decay = decay_seconds_per_unit(1.0, config_.aura_tax);
            if (!pyro.active || pyro.gauge < 1.0) {
                pyro = {true, 1.0, decay, 1.0, target.burning_.owner};
            }
            target.burning_.next_pyro_application += 2.0;
        }
        if (target.burning_.active &&
            target.burning_.next_tick <= target.time_seconds_ + epsilon) {
            emit_periodic(
                Reaction::burning, Element::pyro, target.burning_.owner, true);
            target.burning_.next_tick += config_.burning_tick_seconds;
        }
        if (target.electro_charged_.active &&
            target.electro_charged_.next_tick <= target.time_seconds_ + epsilon) {
            if (target.time_seconds_ - target.electro_charged_.last_damage_time >=
                0.5 - epsilon) {
                emit_periodic(
                    Reaction::electro_charged,
                    Element::electro,
                    target.electro_charged_.owner,
                    true);
                for (const Element element : {Element::hydro, Element::electro}) {
                    auto& aura = target.auras_[index_of(element)];
                    aura.gauge -= config_.electro_charged_gauge_cost;
                }
                target.electro_charged_.last_damage_time = target.time_seconds_;
            }
            target.electro_charged_.next_tick += config_.electro_charged_tick_seconds;
            clear_empty();
        }

    }
    target.time_seconds_ = end_time;
    return events;
}

std::vector<ReactionEvent> ReactionEngine::trigger_dendro_cores(
    TargetState& target,
    Element element,
    const ActorStats& actor,
    std::size_t maximum) const {
    if (element != Element::electro && element != Element::pyro) {
        return {};
    }
    const Reaction reaction =
        element == Element::electro ? Reaction::hyperbloom : Reaction::burgeon;
    const std::size_t count = std::min(maximum, target.cores_.size());
    std::vector<ReactionEvent> events;
    events.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto core = target.cores_.front();
        target.cores_.erase(target.cores_.begin());
        ReactionEvent event;
        event.time_seconds = target.time_seconds_;
        event.reaction = reaction;
        event.damage_element = Element::dendro;
        event.source_id = actor.id;
        event.independent_damage =
            DamageModel::transformative_damage(reaction, Element::dendro, actor, target.stats_);
        event.core_id = core.id;
        events.push_back(std::move(event));
    }
    return events;
}

bool StandardIcdTracker::can_apply(std::string_view group, double time_seconds) {
    auto& state = states_[std::string(group)];
    if (!state.initialized) {
        state = {true, time_seconds, 0};
        return true;
    }
    ++state.hits_since_application;
    const bool time_rule = time_seconds - state.last_application_time >= 2.5 - epsilon;
    const bool hit_rule = state.hits_since_application >= 3;
    if (time_rule || hit_rule) {
        state.last_application_time = time_seconds;
        state.hits_since_application = 0;
        return true;
    }
    return false;
}

void StandardIcdTracker::reset(std::string_view group) {
    states_.erase(std::string(group));
}

void StandardIcdTracker::clear() {
    states_.clear();
}

} // namespace elemental
