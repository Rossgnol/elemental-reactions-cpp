#include "elemental/reaction.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

double parse_replay_speed(int argc, char** argv) {
    if (argc == 1) {
        return 0.0;
    }
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Usage: " << argv[0] << " [--replay-speed number]\n";
        std::cout << "A value of 2 replays the demonstration at 2x speed.\n";
        std::exit(EXIT_SUCCESS);
    }
    if (argc != 3 || std::string(argv[1]) != "--replay-speed") {
        throw std::invalid_argument("expected --replay-speed followed by a positive number");
    }
    const double speed = std::stod(argv[2]);
    if (speed <= 0.0) {
        throw std::invalid_argument("replay speed must be positive");
    }
    return speed;
}

void pause_for(double seconds, double replay_speed) {
    if (replay_speed <= 0.0) {
        return;
    }
    std::this_thread::sleep_for(
        std::chrono::duration<double>(seconds / replay_speed));
}

} // namespace

int main(int argc, char** argv) {
    try {
        using elemental::Element;
        using elemental::ReactionEngine;

        const double replay_speed = parse_replay_speed(argc, argv);

        elemental::ActorStats hydro{"Hydro applicator", 90, 0.0};
        elemental::ActorStats pyro{"Pyro trigger", 90, 180.0};
        pyro.set_bonus(elemental::Reaction::vaporize, 0.15);

        elemental::TargetStats enemy;
        enemy.level = 90;
        for (auto& resistance : enemy.resistance) {
            resistance = 0.10;
        }
        elemental::TargetState target(enemy);
        ReactionEngine engine;

        std::cout << "=== Elemental Reaction Engine: Single-Target Demo ===\n";
        std::cout << "No party, cooldown scheduler, or automatic rotation is used.\n\n";
        pause_for(1.5, replay_speed);

        elemental::Attack setup;
        setup.element = Element::hydro;
        setup.gauge_units = 2.0;
        setup.source = hydro;
        (void)engine.apply(target, setup);

        std::cout << "[1] Apply Hydro aura: 2.00U\n" << std::flush;
        pause_for(2.0, replay_speed);

        elemental::Attack hit;
        hit.element = Element::pyro;
        hit.gauge_units = 1.0;
        hit.source = pyro;
        hit.base_damage = 10'000.0;
        hit.damage_bonus = 0.466;
        hit.critical_multiplier = 2.20;

        std::cout << "[2] Deal a Pyro hit: base damage 10000\n" << std::flush;
        pause_for(2.0, replay_speed);

        const auto result = engine.apply(target, hit);
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& event : result.events) {
            std::cout << "[3] Reaction resolved: " << elemental::to_string(event.reaction)
                      << "\n    Amplifying multiplier: " << event.attack_multiplier
                      << "\n    Independent damage: " << event.independent_damage << '\n';
        }
        pause_for(2.0, replay_speed);
        std::cout << "\nFinal attack damage: " << result.attack_damage << '\n';
        std::cout << "Hydro aura remaining: "
                  << target.aura_gauge(Element::hydro) << "U\n";
        std::cout << "=== Demo complete ===\n" << std::flush;
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
