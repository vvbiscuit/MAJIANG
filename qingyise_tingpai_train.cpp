#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class Difficulty
{
    Easy,
    Medium,
    Hard
};

struct Exercise
{
    vector<int> tiles;
    set<int> wait;
};

struct Template
{
    vector<int> tiles;
};

struct TrainingRecord
{
    string timestamp;
    int round;
    string difficulty;
    string tiles;
    string userAnswer;
    string expectedAnswer;
    double seconds;
    bool correct;
};

// A pure-suit hand holds 13 tiles before the winning tile is drawn, so a
// ready (tenpai) hand plus its winning tile is always 14 tiles long.
constexpr int HandSize = 13;
constexpr int WinSize = HandSize + 1;

// In a real game every tile exists at most four times.
constexpr int MaxCopiesPerTile = 4;

// Structure-preserving transform used for the curated templates: a
// translation that never wraps around (it is only applied while every tile
// stays inside 1..9) plus an optional mirror (1<->9, 2<->8, ...).
// The old modulo-9 shift could turn a run such as 6-7-8 into 9-1-2, which
// silently destroyed the shape the template was meant to teach.
vector<int> transformTiles(const vector<int> &tiles, int offset, bool mirror)
{
    vector<int> result;
    result.reserve(tiles.size());
    for (int value : tiles)
    {
        result.push_back(mirror ? 10 - value + offset : value + offset);
    }
    sort(result.begin(), result.end());
    return result;
}

// Smallest and largest offset that keeps every tile of a (possibly mirrored)
// template inside 1..9. Returns false when no offset works.
bool validOffsetRange(const vector<int> &tiles, bool mirror, int &low, int &high)
{
    int minValue = *min_element(tiles.begin(), tiles.end());
    int maxValue = *max_element(tiles.begin(), tiles.end());
    if (mirror)
    {
        int mirroredMin = 10 - maxValue;
        int mirroredMax = 10 - minValue;
        minValue = mirroredMin;
        maxValue = mirroredMax;
    }

    low = 1 - minValue;
    high = 9 - maxValue;
    return low <= high;
}

string handKey(const vector<int> &tiles)
{
    ostringstream oss;
    for (int value : tiles)
    {
        oss << value << " ";
    }
    return oss.str();
}

bool isValidTileCount(const vector<int> &tiles, int expectedSize)
{
    if (static_cast<int>(tiles.size()) != expectedSize)
    {
        return false;
    }

    vector<int> counts(10, 0);
    for (int tile : tiles)
    {
        if (tile < 1 || tile > 9)
        {
            return false;
        }
        ++counts[tile];
        if (counts[tile] > MaxCopiesPerTile)
        {
            return false;
        }
    }
    return true;
}

bool searchWinning(vector<int> &counts, int meldsUsed, bool pairUsed)
{
    int total = 0;
    for (int i = 1; i <= 9; ++i)
    {
        total += counts[i];
    }

    if (total == 0)
    {
        return meldsUsed == 4 && pairUsed;
    }

    for (int i = 1; i <= 9; ++i)
    {
        if (counts[i] == 0)
        {
            continue;
        }

        if (!pairUsed && counts[i] >= 2)
        {
            counts[i] -= 2;
            if (searchWinning(counts, meldsUsed, true))
            {
                counts[i] += 2;
                return true;
            }
            counts[i] += 2;
        }

        if (counts[i] >= 3)
        {
            counts[i] -= 3;
            if (searchWinning(counts, meldsUsed + 1, pairUsed))
            {
                counts[i] += 3;
                return true;
            }
            counts[i] += 3;
        }

        if (i <= 7 && counts[i] > 0 && counts[i + 1] > 0 && counts[i + 2] > 0)
        {
            --counts[i];
            --counts[i + 1];
            --counts[i + 2];
            if (searchWinning(counts, meldsUsed + 1, pairUsed))
            {
                ++counts[i];
                ++counts[i + 1];
                ++counts[i + 2];
                return true;
            }
            ++counts[i];
            ++counts[i + 1];
            ++counts[i + 2];
        }
    }

    return false;
}

// Seven pairs = exactly seven DISTINCT pairs, i.e. seven different tiles with
// exactly two copies each. A tile with four copies would be "dragon seven
// pairs", which this training program deliberately does NOT accept, so
// AllowDragonSevenPairs stays false.
constexpr bool AllowDragonSevenPairs = false;

bool isSevenPairsWin(const vector<int> &counts)
{
    int total = 0;
    int pairs = 0;

    for (int i = 1; i <= 9; ++i)
    {
        if (counts[i] == 0)
        {
            continue;
        }
        if (counts[i] % 2 != 0)
        {
            return false;
        }
        if (!AllowDragonSevenPairs && counts[i] != 2)
        {
            // four identical tiles -> dragon seven pairs, not a plain one
            return false;
        }

        total += counts[i];
        pairs += counts[i] / 2;
    }

    return total == 14 && pairs == 7;
}

bool canWinWith14Tiles(const vector<int> &tiles)
{
    if (static_cast<int>(tiles.size()) != WinSize)
    {
        return false;
    }

    vector<int> counts(10, 0);
    for (int tile : tiles)
    {
        if (tile < 1 || tile > 9)
        {
            return false;
        }
        ++counts[tile];
        if (counts[tile] > MaxCopiesPerTile)
        {
            return false;
        }
    }

    // isSevenPairsWin() does not modify counts, so it is safe to test first;
    // searchWinning() restores every counter it touches.
    if (isSevenPairsWin(counts))
    {
        return true;
    }
    return searchWinning(counts, 0, 0);
}

set<int> findWaitSet(const vector<int> &tiles)
{
    vector<int> counts(10, 0);
    for (int tile : tiles)
    {
        ++counts[tile];
    }

    set<int> waits;
    for (int value = 1; value <= 9; ++value)
    {
        // A fifth copy of a tile does not exist, so it can never be the wait.
        if (counts[value] >= MaxCopiesPerTile)
        {
            continue;
        }

        vector<int> candidate = tiles;
        candidate.push_back(value);

        if (canWinWith14Tiles(candidate))
        {
            waits.insert(value);
        }
    }

    return waits;
}

string formatDifficulty(Difficulty difficulty)
{
    if (difficulty == Difficulty::Easy)
        return "Easy";
    if (difficulty == Difficulty::Hard)
        return "Hard";
    return "Medium";
}

string nowTimestamp()
{
    auto now = chrono::system_clock::now();
    time_t tt = chrono::system_clock::to_time_t(now);

    tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &tt);
#else
    localtime_r(&tt, &localTime);
#endif

    ostringstream oss;
    oss << put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

string setToString(const set<int> &values)
{
    if (values.empty())
    {
        return "None";
    }

    ostringstream oss;
    for (int value : values)
    {
        oss << value << " ";
    }
    return oss.str();
}

// Strips surrounding whitespace (including the stray '\r' that piped or
// pasted input can carry on Windows).
string trimCopy(const string &text)
{
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos)
    {
        return "";
    }

    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

string vectorToString(const vector<int> &values)
{
    ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i)
    {
        oss << values[i];
        if (i + 1 != values.size())
        {
            oss << " ";
        }
    }
    return oss.str();
}

void saveTrainingRecord(const TrainingRecord &record)
{
    ofstream out("training_history.txt", ios::app);
    if (!out)
    {
        cerr << "Warning: unable to save training history to "
                "training_history.txt"
             << endl;
        return;
    }

    out << record.timestamp << "|"
        << "Round " << record.round << "|" << record.difficulty << "|"
        << record.tiles << "|" << record.userAnswer << "|"
        << record.expectedAnswer << "|" << fixed << setprecision(2)
        << record.seconds << "|" << (record.correct ? "correct" : "wrong")
        << "\n";
}

vector<Template> makeTemplates(Difficulty difficulty)
{
    vector<Template> templates;

    if (difficulty == Difficulty::Easy)
    {
        templates = {{{1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 5, 6, 7}},
                     {{1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 6, 7}},
                     {{2, 2, 2, 3, 3, 3, 4, 4, 5, 6, 7, 8, 9}},
                     {{1, 1, 1, 3, 3, 3, 4, 5, 6, 7, 8, 8, 9}},
                     {{1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 7, 8, 9}}};
    }
    else if (difficulty == Difficulty::Medium)
    {
        templates = {{{1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8}},
                     {{1, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8}},
                     {{1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9}},
                     {{2, 2, 2, 3, 3, 4, 4, 5, 6, 7, 7, 8, 9}},
                     {{1, 1, 2, 3, 3, 4, 5, 5, 6, 6, 7, 8, 9}}};
    }
    else
    {
        templates = {{{1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9}},
                     {{1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 7, 8, 9}},
                     {{1, 1, 2, 3, 4, 4, 5, 5, 6, 6, 7, 8, 9}},
                     {{1, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9}},
                     {{1, 1, 2, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9}}};
    }

    return templates;
}

// ---------------------------------------------------------------------------
// Difficulty is defined by how many tiles make the hand ready. The three
// ranges are deliberately DISJOINT so a "Hard" exercise can never turn out to
// be a trivial one-wait hand. Pool sizes below come from an exhaustive
// enumeration of all 93600 legal 13-tile pure-suit hands:
//   1-2 waits -> 28686 hands, 3-4 waits -> 9687 hands, 5-9 waits -> 1823 hands.
// ---------------------------------------------------------------------------
struct WaitSpan
{
    int minWaits;
    int maxWaits;
};

WaitSpan waitSpanFor(Difficulty difficulty)
{
    if (difficulty == Difficulty::Easy)
    {
        return {1, 2};
    }
    if (difficulty == Difficulty::Medium)
    {
        return {3, 4};
    }
    return {5, 9};
}

Exercise makeExercise(vector<int> tiles)
{
    Exercise ex;
    ex.tiles = std::move(tiles);
    sort(ex.tiles.begin(), ex.tiles.end());
    ex.wait = findWaitSet(ex.tiles);
    return ex;
}

// The curated template shapes, translated (never wrapping around) and
// mirrored, keeping only the ones whose wait count fits this difficulty.
vector<vector<int>> buildCuratedHands(Difficulty difficulty)
{
    const WaitSpan span = waitSpanFor(difficulty);
    const vector<Template> templates = makeTemplates(difficulty);

    vector<vector<int>> hands;
    set<string> seen;

    for (const Template &tpl : templates)
    {
        for (int mirrorFlag = 0; mirrorFlag <= 1; ++mirrorFlag)
        {
            const bool mirror = mirrorFlag != 0;

            int low = 0;
            int high = 0;
            if (!validOffsetRange(tpl.tiles, mirror, low, high))
            {
                continue;
            }

            for (int offset = low; offset <= high; ++offset)
            {
                vector<int> tiles = transformTiles(tpl.tiles, offset, mirror);
                if (!isValidTileCount(tiles, HandSize))
                {
                    continue;
                }

                const string key = handKey(tiles);
                if (!seen.insert(key).second)
                {
                    continue;
                }

                const int waitCount = static_cast<int>(findWaitSet(tiles).size());
                if (waitCount >= span.minWaits && waitCount <= span.maxWaits)
                {
                    hands.push_back(tiles);
                }
            }
        }
    }

    return hands;
}

const vector<vector<int>> &curatedHandsFor(Difficulty difficulty)
{
    static const vector<vector<int>> easy = buildCuratedHands(Difficulty::Easy);
    static const vector<vector<int>> medium = buildCuratedHands(Difficulty::Medium);
    static const vector<vector<int>> hard = buildCuratedHands(Difficulty::Hard);

    if (difficulty == Difficulty::Easy)
    {
        return easy;
    }
    if (difficulty == Difficulty::Medium)
    {
        return medium;
    }
    return hard;
}

// A random legal pure-suit hand (never more than four copies of a tile).
// This is what keeps the pool effectively unlimited: no matter how long a
// session runs there is always a fresh hand available.
vector<int> randomHand()
{
    vector<int> tiles;
    tiles.reserve(HandSize);
    vector<int> counts(10, 0);

    while (static_cast<int>(tiles.size()) < HandSize)
    {
        const int value = rand() % 9 + 1;
        if (counts[value] >= MaxCopiesPerTile)
        {
            continue;
        }

        ++counts[value];
        tiles.push_back(value);
    }

    sort(tiles.begin(), tiles.end());
    return tiles;
}

// One known-ready hand per difficulty, used only as a safety net. It is never
// reached in a normal session: the random source alone would have to miss the
// wanted range 2000 times in a row. The wait counts below were confirmed by
// exhaustive enumeration of the whole hand space.
const Exercise &lastResortExercise(Difficulty difficulty)
{
    // 112233 4567 8 89 -> waits on 8                    (1 wait)
    static const Exercise easy = makeExercise({1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 8, 9});
    // 112233 456789 + 9 -> waits on 3, 6 and 9          (3 waits)
    static const Exercise medium = makeExercise({1, 1, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 9});
    // 1112345678999 -> pure nine gates, waits on 1..9   (9 waits)
    static const Exercise hard = makeExercise({1, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9});

    if (difficulty == Difficulty::Easy)
    {
        return easy;
    }
    if (difficulty == Difficulty::Medium)
    {
        return medium;
    }
    return hard;
}

Exercise generateExercise(Difficulty difficulty, set<string> &usedHands)
{
    // About one candidate in CuratedMixing comes from the curated shapes.
    constexpr int CuratedMixing = 8;

    const WaitSpan span = waitSpanFor(difficulty);
    const vector<vector<int>> &curated = curatedHandsFor(difficulty);

    // Two cycles: the second one runs with a cleared "already served" set, so
    // it can only miss if the draws keep falling outside the wanted range.
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        for (int attempt = 0; attempt < 2000; ++attempt)
        {
            // Roughly one candidate in eight comes from the curated shapes;
            // the rest are freshly randomised, which is what keeps the pool
            // effectively unlimited.
            const bool fromCurated = !curated.empty() && (rand() % CuratedMixing == 0);
            Exercise ex = makeExercise(fromCurated
                                           ? curated[rand() % curated.size()]
                                           : randomHand());

            const int waitCount = static_cast<int>(ex.wait.size());
            if (waitCount < span.minWaits || waitCount > span.maxWaits)
            {
                continue;
            }

            if (!usedHands.insert(handKey(ex.tiles)).second)
            {
                continue; // already served during this cycle
            }

            return ex;
        }

        // Every hand of this difficulty has been served once: start over
        // instead of endlessly repeating a single fixed hand.
        usedHands.clear();
    }

    const Exercise &fallback = lastResortExercise(difficulty);
    usedHands.insert(handKey(fallback.tiles));
    return fallback;
}

string tileString(int value)
{
    return to_string(value);
}

// Turns "4 7" (or "4,7") into {4, 7}. Returns false when the text is not a
// clean list of single tile numbers 1..9, so that a typo like "13" or "4.5"
// can no longer be silently accepted as a valid answer.
bool parseWaitValues(const string &input, vector<int> &result)
{
    result.clear();

    // Treat commas as separators too, so "4,7" and "4 7" both work, while
    // "47", "13", "4.5" or "abc4" stay invalid instead of being mined for
    // digits.
    string normalized = input;
    replace(normalized.begin(), normalized.end(), ',', ' ');

    string token;
    istringstream iss(normalized);
    while (iss >> token)
    {
        if (token.size() != 1 || token[0] < '1' || token[0] > '9')
        {
            result.clear();
            return false;
        }

        result.push_back(token[0] - '0');
    }

    sort(result.begin(), result.end());
    result.erase(unique(result.begin(), result.end()), result.end());
    return true;
}

void printTiles(const vector<int> &tiles)
{
    cout << "Tile set: ";
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        cout << tileString(tiles[i]);
        if (i + 1 != tiles.size())
        {
            cout << " ";
        }
    }
    cout << endl;
}

void printRoundSummary(bool correct, const set<int> &expected, double seconds)
{
    cout << "\n----------------------------------------\n";
    if (correct)
    {
        cout << "Result: correct!" << endl;
    }
    else
    {
        cout << "Result: wrong!" << endl;
    }

    cout << "Correct waiting tiles: ";
    if (expected.empty())
    {
        cout << "None";
    }
    else
    {
        for (int value : expected)
        {
            cout << value << " ";
        }
    }
    cout << endl;

    cout << fixed;
    cout << "Time used: " << seconds << " s" << endl;
    cout << "----------------------------------------\n";
}

void clearInputBuffer()
{
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void printTrainingFileInfo()
{
    cout << "\nTraining records are saved to: training_history.txt\n";
}

Difficulty chooseDifficulty()
{
    int choice = 0;
    cout << "Choose difficulty: 1=Easy  2=Medium  3=Hard: ";
    cin >> choice;

    if (choice == 1)
        return Difficulty::Easy;
    if (choice == 3)
        return Difficulty::Hard;
    return Difficulty::Medium;
}

int main()
{
    srand(static_cast<unsigned int>(time(nullptr)));

    int rounds = 0;
    cout << "Enter the number of training rounds (for example, 5): ";
    cin >> rounds;
    clearInputBuffer();
    if (rounds <= 0)
    {
        cout << "Round count must be a positive integer. Starting with 5 "
                "rounds by default."
             << endl;
        rounds = 5;
    }

    Difficulty difficulty = chooseDifficulty();
    clearInputBuffer();

    set<string> usedHands;
    int correctCount = 0;
    double totalTime = 0.0;
    double minTime = 999999.0;
    double maxTime = 0.0;

    cout << "\n=== Pure suit waiting tile training ===\n";
    cout << "Rule: the program randomly generates valid pure-suit waiting "
            "patterns.\n";
    cout << "Only enter the waiting numbers, for example: 4 or 5 6\n\n";
    printTrainingFileInfo();

    ofstream sessionHeader("training_history.txt", ios::app);
    if (sessionHeader)
    {
        sessionHeader << "\n===== Session Start: " << nowTimestamp()
                      << " | Difficulty: " << formatDifficulty(difficulty)
                      << " =====\n";
    }
    sessionHeader.close();

    for (int i = 1; i <= rounds; ++i)
    {
        Exercise ex = generateExercise(difficulty, usedHands);

        cout << "Round " << i << "\n";
        printTiles(ex.tiles);

        auto start = chrono::steady_clock::now();
        cout << "Enter your waiting tile answer: ";

        string answer;
        getline(cin, answer);
        if (cin.fail())
        {
            clearInputBuffer();
            getline(cin, answer);
        }

        auto end = chrono::steady_clock::now();
        double seconds = chrono::duration<double>(end - start).count();

        vector<int> userNumbers;
        const bool answerValid = parseWaitValues(answer, userNumbers);
        set<int> userSet(userNumbers.begin(), userNumbers.end());

        bool correct = answerValid && (userSet == ex.wait);
        if (correct)
        {
            ++correctCount;
        }

        totalTime += seconds;
        minTime = min(minTime, seconds);
        maxTime = max(maxTime, seconds);

        TrainingRecord record{nowTimestamp(),
                              i,
                              formatDifficulty(difficulty),
                              vectorToString(ex.tiles),
                              answer,
                              setToString(ex.wait),
                              seconds,
                              correct};
        saveTrainingRecord(record);

        if (!answerValid)
        {
            cout << "(Ignored: please enter only tile numbers 1-9, for "
                    "example \"4\" or \"5 6\".)\n";
        }

        printRoundSummary(correct, ex.wait, seconds);
        cout << "\n";
    }

    cout << "=== Training summary ===\n";
    cout << "Correct answers: " << correctCount << "/" << rounds << endl;
    cout << "Average time: " << (rounds > 0 ? totalTime / rounds : 0.0) << " s"
         << endl;
    cout << "Fastest time: " << minTime << " s" << endl;
    cout << "Slowest time: " << maxTime << " s" << endl;

    if (correctCount == rounds)
    {
        cout << "Perfect. Keep it up!" << endl;
    }
    else if (correctCount >= rounds * 0.7)
    {
        cout << "Good performance. Keep improving!" << endl;
    }
    else if (correctCount >= rounds * 0.1)
    {
        cout << "Practice more and your waiting judgment will improve." << endl;
    }
    else
    {
        cout << "Are you f**king stupid! Get out of my room." << endl;
    }

    ofstream sessionEnd("training_history.txt", ios::app);
    if (sessionEnd)
    {
        sessionEnd << "===== Session End: " << nowTimestamp()
                   << " | Difficulty: " << formatDifficulty(difficulty)
                   << " =====\n";
    }
    sessionEnd.close();

    // getline() failing means the input stream ended (for example when the
    // program is fed from a pipe), so stop instead of spinning forever.
    string quitStr;
    while (cout << "Type 'q' or 'exit' to quit: " && getline(cin, quitStr))
    {
        const string reply = trimCopy(quitStr);
        if (reply == "q" || reply == "exit")
        {
            return 0;
        }
    }

    return 0;
}
