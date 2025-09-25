#include <sstream>
#include <string>
#include <vector>

extern std::string Py_IndentJoin(std::vector<std::string> lines, int indent_level);
extern std::string Py_QuoteString(const std::string& str);

enum class PyOptionType {
    // Generic option types
    Removed,
    InID1Common,

    Toggle,
    Choice,
    Range,
    OptionSet,

    CheckSanity,
    Episode,
    InvisibilityTrap,
    StartWithMaps,
    CapacitySplit,
    CapacityCount,
};

struct PyOption {
public:
    std::vector<std::string> docstring;

    int default_int = -9999;
    std::vector<std::string> default_list;

    // Set or Choice options
    std::vector<std::string> option_list;

    // Special generic options
    int doom_type = 0;

    // Range options
    int range_start = 0, range_end = 0;

    // Episode options
    bool is_minor_episode = false;

    // Capacity split options
    int split_item_count = 0;

    // Option Group to place option in
    std::string option_group;

private:
    PyOptionType type;
    std::string data_name;
    std::string display_name;

    bool has_own_class;
    std::string class_name;

public:
    PyOption(const std::string &dname, PyOptionType type)
        : type(type), data_name(dname)
    {
        has_own_class = false;
    }
    PyOption(const std::string &dname, const std::string &name, PyOptionType type)
        : type(type), data_name(dname), display_name(name)
    {
        has_own_class = true;
        switch (type)
        {
        default:
            break;
        case PyOptionType::Removed:
            has_own_class = false;
            return;
        case PyOptionType::InID1Common:
            class_name = name;
            has_own_class = false;
            return;
        }

        // Convert data_name to a ClassName
        bool next_upper = true;
        for (char c : data_name)
        {
            if (c >= '0' && c <= '9')
            {
                class_name += c;
                next_upper = true;
            }
            else if (c >= 'A' && c <= 'Z')
            {
                class_name += c;
                next_upper = false;
            }
            else if (c >= 'a' && c <= 'z')
            {
                class_name += c + (next_upper ? (-'a'+'A') : 0);
                next_upper = false;                
            }
            else
                next_upper = true;
        }
    }

    std::string GetBaseClass(void) const
    {
        switch (type)
        {
        case PyOptionType::Removed:
            return "BaseOptions.Removed";
        case PyOptionType::InID1Common:
            return "id1Options." + class_name;
        case PyOptionType::Toggle:
            return (default_int ? "BaseOptions.DefaultOnToggle" : "BaseOptions.Toggle");
        case PyOptionType::Choice:
            return "BaseOptions.Choice";
        case PyOptionType::Range:
            return "BaseOptions.Range";
        case PyOptionType::OptionSet:
            return "BaseOptions.OptionSet";
        case PyOptionType::CheckSanity:
            return "id1Options.CheckSanity";
        case PyOptionType::Episode:
            if (is_minor_episode)
                return (default_int ? "id1Options.MinorDefaultEpisode" : "id1Options.MinorEpisode");
            return (default_int ? "id1Options.DefaultEpisode" : "id1Options.Episode");
        case PyOptionType::InvisibilityTrap:
            return "id1Options.PartialInvisibilityAsTrap";
        case PyOptionType::StartWithMaps:
            return "id1Options.StartWithComputerAreaMaps";
        case PyOptionType::CapacitySplit:
            return (split_item_count == 6 ? "id1Options.SplitBagOfHolding" : "id1Options.SplitBackpack");
        case PyOptionType::CapacityCount:
            return "id1Options.BackpackCount";
        }
        return "ERROR";
    }

    std::string GetClassName(void) const
    {
        return (has_own_class ? class_name : GetBaseClass());
    }

    std::string OutputClass(void) const
    {
        if (!has_own_class)
            return "";

        std::stringstream output;
        output << "class " << class_name << "(" << GetBaseClass() << "):" << std::endl;
        output << "    \"\"\"" << std::endl;
        output << Py_IndentJoin(docstring, 4);
        output << "    \"\"\"" << std::endl;
        output << "    display_name = " << Py_QuoteString(display_name) << std::endl;

        if (doom_type)
            output << "    doom_type = " << std::to_string(doom_type) << std::endl;

        switch (type)
        {
        default:
            break;
        case PyOptionType::Choice:
            for (const std::string &opt : option_list)
            {
                if (opt.find_first_of('=') != std::string::npos)
                    output << "    " << opt << std::endl;
            }
            output << "    default = " << std::to_string(default_int != -9999 ? default_int : 0) << std::endl;
            break;
        case PyOptionType::Range:
            output << "    range_start = " << std::to_string(range_start) << std::endl; 
            output << "    range_end = " << std::to_string(range_end) << std::endl;
            output << "    default = " << std::to_string(default_int != -9999 ? default_int : range_end) << std::endl;
            break;
        case PyOptionType::OptionSet:
            output << "    valid_keys = frozenset({" << std::endl;
            for (const std::string &opt : option_list)
                output << "        " << Py_QuoteString(opt) << "," << std::endl;
            output << "    })" << std::endl;
            output << "    default = frozenset({" << std::endl;
            for (const std::string &opt : default_list)
                output << "        " << Py_QuoteString(opt) << "," << std::endl;
            output << "    })" << std::endl;            
            break;
        case PyOptionType::CapacitySplit:
            if (split_item_count != 4 && split_item_count != 6)
            {
                output << "    split_doom_types = [";
                for (int i = 1; i <= split_item_count; ++i)
                {
                    if (i != 1)
                        output << ", ";
                    output << std::to_string(65000+i); 
                }
                output << "]" << std::endl;
            }
            break;
        }

        output << std::endl << std::endl;
        return output.str();
    }

    std::string OutputDataclass(void) const
    {
        std::stringstream output;
        output << data_name << ": " << GetClassName();
        if (data_name == "goal_num_levels" || data_name == "goal_specific_levels" || data_name == "flip_levels")
            output << "  # type: ignore[assignment]";
        output << std::endl;
        return output.str();
    }
};

class game_t;

extern std::stringstream& Py_CreateInitPy(game_t *game);
extern std::stringstream& Py_CreateOptionsPy(game_t *game, std::vector<PyOption>& opts);

// WorldOptions; it just winds up being most convenient to have these here
extern int WorldOptions_Init(game_t *game);
extern std::vector<std::string>& WorldOptions_GetAllHooks(game_t *game, const std::string& hook_type, int line_breaks = 1);
extern void WorldOptions_MixinPyOptions(game_t *game, std::vector<PyOption>& opts);
