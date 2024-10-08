#include "api.h"
#include "utils.h"
#include <iostream>
#include <random>
#include <spdlog/spdlog.h>
#include <string>

using namespace cycles;

class BotClient {
  Connection connection;
  std::string name;
  GameState state;
  Player my_player;
  std::mt19937 rng;
  int previousDirection = -1;
  int inertia = 30;

  bool is_valid_move(Direction direction) {
    // Check that the move does not overlap with any grid cell that is set to
    // not 0
    auto new_pos = my_player.position + getDirectionVector(direction);
    if (!state.isInsideGrid(new_pos)) {
      return false;
    }
    if (state.getGridCell(new_pos) != 0) {
      return false;
    }
    return true;
  }

  Direction decideMove() {
      constexpr int max_attempts = 200;
      int attempts = 0;
      const auto position = my_player.position;
      const int frameNumber = state.frameNumber;

      // Get the direction scores
      auto direction_scores = scoreDirections();
      Direction best_direction;

      do {
          if (attempts >= max_attempts) {
              spdlog::error("{}: Failed to find a valid move after {} attempts", name, max_attempts);
              exit(1);
          }

          // Find the direction with the highest score (most open space)
          best_direction = std::max_element(
              direction_scores.begin(),
              direction_scores.end(),
              [](const std::pair<Direction, int>& a, const std::pair<Direction, int>& b) {
                  return a.second < b.second;  // Compare scores
              }
          )->first;

          // If inertia is still valid, favor previous direction
          if (inertia > 0 && previousDirection == getDirectionValue(best_direction)) {
              inertia--;
          }

          attempts++;
      } while (!is_valid_move(best_direction));  // Ensure the move is valid

      spdlog::debug("{}: Valid move found after {} attempts, moving from ({}, {}) to ({}, {}) in frame {}",
                    name, position.x, position.y, attempts,
                    position.x + getDirectionVector(best_direction).x,
                    position.y + getDirectionVector(best_direction).y, frameNumber);

      previousDirection = getDirectionValue(best_direction);
      return best_direction;
  }

  // With this function I'm updating the inertia when I receive
  // the game state depending on the direction scores
  void updateInertia() {

    // Again, I'm getting all scores
    auto direction_scores = scoreDirections();
  
    // Here I'm getting the element withe the best score.
    // I'm using a lambda function to determine how max_element
    // will compare the two directions
    // I'm using .second to refer to the value and not the key in the map
    auto max_score_iter = std::max_element(
        direction_scores.begin(),
        direction_scores.end(),
        [](const std::pair<Direction, int>& a, const std::pair<Direction, int>& b) {
            return a.second < b.second;
        }
    );
    
    int max_open_space = max_score_iter->second;

    // Reduce inertia if the bot is near walls or players (few open cells)
    if (max_open_space < 2) {
        inertia = std::max(0, inertia - 1);
    } else {
        inertia = std::min(30, inertia + 1);  // Increase inertia in open spaces
    }
}

  // For this func, I'm going through all directions and checking 5 cells ahead
  // If there is no block, I'm adding 1 to the direction score.

  // At the end this allows me to determine the "freest" path based on the score
  std::map<Direction, int> scoreDirections() {

    // This func will return a map
    std::map<Direction, int> scores;

    // I'm going through all directions
    for (Direction direction : {Direction::north, Direction::east, Direction::south, Direction::west}) {
        int score = 0;
        auto next_pos = my_player.position;

        // Move in the chosen direction and count open cells, for each open cell
        // I'm adding 1 to the score of each direction
        for (int i = 0; i < 5; ++i) { 
            next_pos += getDirectionVector(direction);

            // I'm checking if the player will hit a wall or a player in the next 5 cells ahead
            if (!state.isInsideGrid(next_pos) || state.getGridCell(next_pos) != 0) {
                break;
            }
            score++;
        }
        scores[direction] = score;
    }
    return scores;
  }

  void receiveGameState() {
    state = connection.receiveGameState();
    updateInertia();
    for (const auto &player : state.players) {
      if (player.name == name) {
        my_player = player;
        break;
      }
    }
  }

  void sendMove() {
    spdlog::debug("{}: Sending move", name);
    auto move = decideMove();
    previousDirection = getDirectionValue(move);
    connection.sendMove(move);
  }

public:
  BotClient(const std::string &botName) : name(botName) {
    std::random_device rd;
    rng.seed(rd());
    std::uniform_int_distribution<int> dist(0, 50);
    inertia = dist(rng);
    connection.connect(name);
    if (!connection.isActive()) {
      spdlog::critical("{}: Connection failed", name);
      exit(1);
    }
  }

  void run() {
    while (connection.isActive()) {
      receiveGameState();
      sendMove();
    }
  }

};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <bot_name>" << std::endl;
    return 1;
  }
#if SPDLOG_ACTIVE_LEVEL == SPDLOG_LEVEL_TRACE
  spdlog::set_level(spdlog::level::debug);
#endif
  std::string botName = argv[1];
  BotClient bot(botName);
  bot.run();
  return 0;
}
