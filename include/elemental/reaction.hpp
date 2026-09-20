#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elemental {

enum class Element : std::uint8_t {
    physical,
    pyro,
    hydro,
    electro,
    cryo,
    anemo,
    geo,
    dendro,
    count
};

enum class Reaction : std::uint8_t {
    none,
    vaporize,
    melt,
    overloaded,
    electro_charged,
    superconduct,
    frozen,
    shatter,
    swirl,
    crystallize,
    burning,
    bloom,
    bloom_explosion,
    hyperbloom,
    burgeon,
    quicken,
    aggravate,
    spread,
    lunar_charged,
    lunar_bloom,
    lunar_crystallize,
    count
};

constexpr std::size_t element_count = static_cast<std::size_t>(Element::count);
constexpr std::size_t reaction_count = static_cast<std::size_t>(Reaction::count);

std::string_view to_string(Element element) noexcept;
std::string_view to_string(Reaction reaction) noexcept;

struct ActorStats {
    std::string id{"anonymous"};
    int level{90};
    double elemental_mastery{0.0};
    std::array<double, reaction_count> reaction_bonus{};

    [[nodiscard]] double bonus(Reaction reaction) const noexcept;
    void set_bonus(Reaction reaction, double value) noexcept;
};

struct TargetStats {
    int level{90};
    std::array<double, element_count> resistance{};
    double freeze_resistance{0.0};
    double defense_reduction{0.0};

    [[nodiscard]] double resistance_to(Element element) const noexcept;
    void set_resistance(Element element, double value) noexcept;
};

struct Attack {
    Element element{Element::physical};
    double gauge_units{0.0};
    ActorStats source{};
    bool applies_aura{true};
    bool blunt{false};

    // base_damage is the talent/stat-scaled value before DMG%, CRIT, DEF, RES,
    // and amplifying reactions. Leave it at zero for aura-only simulations.
    double base_damage{0.0};
    double damage_bonus{0.0};
    double critical_multiplier{1.0};
    double defense_ignore{0.0};
};

struct ReactionEvent {
    double time_seconds{0.0};
    Reaction reaction{Reaction::none};
    Element damage_element{Element::physical};
    std::string source_id{};
    double independent_damage{0.0};
    double attack_multiplier{1.0};
    double additive_base_damage{0.0};
    double shield_strength{0.0};
    double duration_seconds{0.0};
    double gauge_consumed{0.0};
    int core_id{0};
    bool periodic{false};
    bool damage_suppressed{false};
    std::string note{};
};

struct Resolution {
    std::vector<ReactionEvent> events{};
    double attack_multiplier{1.0};
    double additive_base_damage{0.0};
    double attack_damage{0.0};
};

struct LunarContributor {
    ActorStats stats{};
    double critical_multiplier{1.0};
};

class DamageModel {
public:
    [[nodiscard]] static double player_level_multiplier(int level);
    [[nodiscard]] static double crystallize_level_multiplier(int level);
    [[nodiscard]] static double resistance_multiplier(double resistance) noexcept;
    [[nodiscard]] static double defense_multiplier(
        int attacker_level,
        int target_level,
        double defense_reduction = 0.0,
        double defense_ignore = 0.0) noexcept;
    [[nodiscard]] static double amplifying_multiplier(
        Reaction reaction,
        Element trigger,
        const ActorStats& actor);
    [[nodiscard]] static double additive_base_damage(
        Reaction reaction,
        const ActorStats& actor);
    [[nodiscard]] static double transformative_damage(
        Reaction reaction,
        Element damage_element,
        const ActorStats& actor,
        const TargetStats& target);
    [[nodiscard]] static double crystallize_shield(const ActorStats& actor);
    [[nodiscard]] static double attack_damage(
        const Attack& attack,
        const TargetStats& target,
        double additive_base_damage,
        double amplifying_multiplier);

    // Lunar reactions are conditional conversions supplied by character
    // passives. These helpers model their shared damage equations; talent
    // resource generation and character-specific conversion remain game-layer
    // responsibilities.
    [[nodiscard]] static double lunar_indirect_damage(
        Reaction reaction,
        const std::vector<LunarContributor>& contributors,
        const TargetStats& target,
        double lunar_base_damage_bonus = 0.0,
        double elevation_multiplier = 1.0);
    [[nodiscard]] static double lunar_direct_damage(
        Reaction reaction,
        double scaling_stat,
        double talent_multiplier,
        const LunarContributor& contributor,
        const TargetStats& target,
        double lunar_base_damage_bonus = 0.0,
        double elevation_multiplier = 1.0);
};

struct EngineConfig {
    double aura_tax{0.8};
    double electro_charged_tick_seconds{1.0};
    double electro_charged_gauge_cost{0.4};
    double burning_tick_seconds{0.25};
    double burning_dendro_minimum_cost_per_second{0.4};
    double dendro_core_lifetime_seconds{6.0};
    std::size_t maximum_dendro_cores{5};
};

class TargetState {
public:
    explicit TargetState(TargetStats stats = {});

    [[nodiscard]] TargetStats& stats() noexcept;
    [[nodiscard]] const TargetStats& stats() const noexcept;
    [[nodiscard]] double time_seconds() const noexcept;
    [[nodiscard]] double aura_gauge(Element element) const noexcept;
    [[nodiscard]] double aura_remaining_seconds(Element element) const noexcept;
    [[nodiscard]] bool is_frozen() const noexcept;
    [[nodiscard]] bool is_burning() const noexcept;
    [[nodiscard]] bool is_electro_charged() const noexcept;
    [[nodiscard]] bool is_quickened() const noexcept;
    [[nodiscard]] double quicken_remaining_seconds() const noexcept;
    [[nodiscard]] double superconduct_remaining_seconds() const noexcept;
    [[nodiscard]] std::size_t dendro_core_count() const noexcept;

private:
    struct AuraSlot {
        bool active{false};
        double gauge{0.0};
        double decay_seconds_per_unit{0.0};
        double source_gauge_units{0.0};
        ActorStats owner{};
    };

    struct FrozenStatus {
        bool active{false};
        double gauge{0.0};
        double elapsed_seconds{0.0};
    };

    struct QuickenStatus {
        bool active{false};
        double gauge{0.0};
        double decay_seconds_per_unit{0.0};
        ActorStats owner{};
    };

    struct BurningStatus {
        bool active{false};
        double gauge{0.0};
        double next_tick{0.0};
        double next_pyro_application{0.0};
        ActorStats owner{};
    };

    struct ElectroChargedStatus {
        bool active{false};
        double next_tick{0.0};
        double last_damage_time{-std::numeric_limits<double>::infinity()};
        ActorStats owner{};
    };

    struct DendroCore {
        int id{0};
        double expires_at{0.0};
        ActorStats owner{};
    };

    TargetStats stats_{};
    double time_seconds_{0.0};
    std::array<AuraSlot, element_count> auras_{};
    FrozenStatus frozen_{};
    QuickenStatus quicken_{};
    BurningStatus burning_{};
    ElectroChargedStatus electro_charged_{};
    double superconduct_remaining_seconds_{0.0};
    double crystallize_cooldown_until_{0.0};
    std::vector<DendroCore> cores_{};
    int next_core_id_{1};

    friend class ReactionEngine;
};

class ReactionEngine {
public:
    explicit ReactionEngine(EngineConfig config = {});

    [[nodiscard]] const EngineConfig& config() const noexcept;
    [[nodiscard]] Resolution apply(TargetState& target, const Attack& attack) const;
    [[nodiscard]] std::vector<ReactionEvent> advance(
        TargetState& target,
        double seconds) const;
    [[nodiscard]] std::vector<ReactionEvent> trigger_dendro_cores(
        TargetState& target,
        Element trigger,
        const ActorStats& actor,
        std::size_t maximum_to_trigger = std::numeric_limits<std::size_t>::max()) const;

private:
    EngineConfig config_{};
};

// Utility for attack sources that follow the common 3-hit / 2.5-second
// application rule. Separate group keys model independent ICD groups.
class StandardIcdTracker {
public:
    [[nodiscard]] bool can_apply(std::string_view group, double time_seconds);
    void reset(std::string_view group);
    void clear();

private:
    struct State {
        bool initialized{false};
        double last_application_time{0.0};
        int hits_since_application{0};
    };
    std::unordered_map<std::string, State> states_{};
};

} // namespace elemental
