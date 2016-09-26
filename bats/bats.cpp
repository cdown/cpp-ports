#include <set>
#include <dirent.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <sstream>
#include <fstream>
#include <vector>

std::string power_supply_path = "/sys/class/power_supply/";
const char* batt_prefix = "BAT";
const std::vector<std::string> charge_prefixes = {"charge_", "energy_"};
const char* verbose_flag = "-v";

auto get_all_battery_names();
auto get_status_string(auto battery_names);
auto make_battery_path(auto battery_name, auto filename);
auto read_battery_file(auto battery_name, auto filename,
                       std::streamsize count=0);
auto read_charge_or_energy(auto battery_name, auto filename);
auto  read_battery_file_int(auto battery_name, auto filename);

auto get_all_battery_names() {
    DIR *power_supply_dir;
    struct dirent *batt_dir;
    std::set<std::string> matching_dirs;

    power_supply_dir = opendir(power_supply_path.c_str());

    if (power_supply_dir == NULL) {
        // TODO: is this a reasonable way to handle this?
        throw std::runtime_error(power_supply_path + ": " + strerror(errno));
    }

    while ((batt_dir = readdir(power_supply_dir)) != NULL) {
        if (!strncmp(batt_prefix, batt_dir->d_name, strlen(batt_prefix))) {
            matching_dirs.insert(batt_dir->d_name);
        }
    }

    closedir(power_supply_dir);

    return matching_dirs;
}

auto make_battery_path(auto battery_name, auto filename) {
    return power_supply_path + battery_name + "/" + filename;
}

auto read_battery_file(auto battery_name, auto filename, std::streamsize count) {
    auto full_battery_path = make_battery_path(battery_name, filename);
    std::stringstream battery_f_buf;
    std::string file_data;
    char battery_f_char[count + 1];
    std::ifstream battery_f(full_battery_path);

    if (!battery_f) {
        throw std::runtime_error(full_battery_path + ": " + strerror(errno));
    }

    // TODO: Some way to dedupe these two methods?
    if (count <= 0) {
        // Cannot use buffered reads or other token based functions as files in
        // /sys only allow one read
        battery_f_buf << battery_f.rdbuf();
        file_data = battery_f_buf.str();
    } else {
        battery_f.read(battery_f_char, sizeof(battery_f_char) - 1);
        file_data = std::string(battery_f_char);
    }

    return file_data;
}

auto read_charge_or_energy(auto battery_name, auto filename) {
    std::stringstream error_msg(
        "Neither energy or charge prefix exists for ",
        std::ios_base::app | std::ios_base::out
    );
    error_msg << battery_name << "/" << filename;

    for (auto prefix : charge_prefixes) {
        try {
            return read_battery_file(battery_name, prefix + filename);
        } catch(std::runtime_error) {
            continue;
        }
    }

    throw std::runtime_error(error_msg.str());
}

auto read_battery_file_int(auto battery_name, auto filename) {
    auto raw_file_str = read_charge_or_energy(battery_name, filename);
    raw_file_str.erase(raw_file_str.find_last_not_of("\n") + 1);
    return std::stol(raw_file_str);
}

int main(int argc, char* argv[]) {
    std::set<std::string> battery_names(argv + 1, argv + argc);
    long total_charge_full = 0;
    long total_charge_now = 0;
    std::string status_output;
    bool verbose = false;

    if (battery_names.find(verbose_flag) != battery_names.end()) {
        verbose = true;
        battery_names.erase(verbose_flag);
    }

    if (battery_names.empty()) {
        battery_names = get_all_battery_names();
    }

    for (auto battery_name : battery_names) {
        status_output += read_battery_file(battery_name, "status", 1);
        total_charge_full += read_battery_file_int(battery_name, "full");
        total_charge_now += read_battery_file_int(battery_name, "now");
    }


    long percentage = (total_charge_now * 100 / total_charge_full);

    if (verbose) {
        std::cout << "verbose" << std::endl;
    }

    std::cout << percentage << status_output << std::endl;
}
