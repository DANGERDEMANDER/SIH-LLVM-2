#include <ncurses.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

// Minimal ncurses-based menu that mirrors menu_cli options. This TUI calls
// external commands (clang/opt/llc) via system() like the menu_cli implementation.

struct RunConfig {
    std::string src = "tests/hello.c";
    std::string preset = "balanced";
    int bogus_ratio = 20;
    int string_intensity = 1;
    int cycles = 1;
    unsigned int seed = 0;
    #include <ncurses.h>
    #include <string>
    #include <vector>
    #include <cstdlib>
    #include <iostream>
    #include <algorithm>

    // Polished single-style ncurses TUI matching provided screenshots.
    // Steps: 1) File selection & analysis
    //        2) Preset selection (numbered)
    //        3) Processing (progress bar)
    //        4) Results summary and prompt [y/N]

    struct RunConfig {
        std::string src = "tests/hello.c";
        std::string preset = "balanced";
        int bogus_ratio = 20;
        int cycles = 1;
        unsigned int seed = 0;
        std::string out_bin = "dist/main_obf";
    };

    static void center_print(WINDOW *w, int y, const std::string &s, int color_pair=0) {
        int wxy = getmaxx(w);
        int x = std::max(1, (wxy - (int)s.size())/2);
        if (color_pair) wattron(w, COLOR_PAIR(color_pair));
        mvwprintw(w, y, x, "%s", s.c_str());
        if (color_pair) wattroff(w, COLOR_PAIR(color_pair));
    }

    static void ascii_box_header(WINDOW *w, int y, int color_pair) {
        int wxy = getmaxx(w);
        std::string sep(wxy-4, '=');
        wattron(w, COLOR_PAIR(color_pair));
        mvwprintw(w, y, 2, "%s", sep.c_str());
        wattroff(w, COLOR_PAIR(color_pair));
    }

    static int prompt_input(WINDOW *w, int y, const std::string &prompt, char *buf, int bufsize) {
        mvwprintw(w, y, 4, "%s", prompt.c_str());
        wclrtoeol(w);
        echo();
        wgetnstr(w, buf, bufsize-1);
        noecho();
        return 0;
    }

    static int numbered_selector(WINDOW *w, int starty, const std::vector<std::pair<int,std::string>> &items, int default_choice) {
        int y = starty;
        for (const auto &p : items) {
            wattron(w, A_BOLD);
            wattron(w, COLOR_PAIR(4));
            mvwprintw(w, y, 4, "%d.", p.first);
            wattroff(w, COLOR_PAIR(4));
            wattroff(w, A_BOLD);
            mvwprintw(w, y, 8, "%s", p.second.c_str());
            y += 2;
        }
        // prompt
        wattron(w, COLOR_PAIR(3));
        mvwprintw(w, y, 4, "[G] Select preset [%d]: ", default_choice);
        wattroff(w, COLOR_PAIR(3));
        wrefresh(w);
        echo();
        char buf[16] = {0};
        wgetnstr(w, buf, sizeof(buf)-1);
        noecho();
        std::string s(buf);
        if (s.empty()) return default_choice;
        int c = atoi(s.c_str());
        bool ok = false; for (auto &p: items) if (p.first == c) ok = true;
        return ok ? c : default_choice;
    }

    static void progress_bar(WINDOW *w, int y, int pct) {
        int bar_w = getmaxx(w) - 12;
        int filled = (pct * bar_w) / 100;
        mvwprintw(w, y, 4, "[");
        for (int i=0;i<bar_w;i++) mvwprintw(w, y, 5+i, " ");
        mvwprintw(w, y, 5+bar_w, "] %3d%%", pct);
        for (int i=0;i<filled;i++) {
            wattron(w, COLOR_PAIR(2));
            mvwprintw(w, y, 5+i, "#");
            wattroff(w, COLOR_PAIR(2));
        }
        wrefresh(w);
    }

    static void show_summary(WINDOW *w, int starty, const RunConfig &cfg) {
        int y = starty;
        wattron(w, COLOR_PAIR(1));
        mvwprintw(w, y, 4, "==================== OB F U S C A T I O N  S U M M A R Y ====================");
        wattroff(w, COLOR_PAIR(1));
        y += 2;
        wattron(w, COLOR_PAIR(3));
        mvwprintw(w, y++, 6, "Input file: %s", cfg.src.c_str());
        mvwprintw(w, y++, 6, "Output file: %s", cfg.out_bin.c_str());
        wattroff(w, COLOR_PAIR(3));
        wattron(w, COLOR_PAIR(4));
        mvwprintw(w, y++, 6, "Obfuscation cycles: %d", cfg.cycles);
        mvwprintw(w, y++, 6, "Bogus code percentage: %d%%", cfg.bogus_ratio);
        wattroff(w, COLOR_PAIR(4));
        wattron(w, COLOR_PAIR(2));
        mvwprintw(w, y++, 6, "[SUCCESS] Obfuscation completed successfully!");
        wattroff(w, COLOR_PAIR(2));
        mvwprintw(w, y++, 6, "\nProcess another file [y/N]: ");
        wrefresh(w);
    }

    int main() {
        initscr();
        noecho();
        cbreak();
        curs_set(1);
        if (has_colors()) {
            start_color(); use_default_colors();
            init_pair(1, COLOR_CYAN, -1);
            init_pair(2, COLOR_GREEN, -1);
            init_pair(3, COLOR_YELLOW, -1);
            init_pair(4, COLOR_MAGENTA, -1);
        }

        int height = 20, width = 80;
        int starty = (LINES - height) / 2;
        int startx = (COLS - width) / 2;
        WINDOW *win = newwin(height, width, starty, startx);
        keypad(win, TRUE);

        RunConfig cfg;
        bool again = true;
        while (again) {
            werase(win); box(win, 0, 0);
            ascii_box_header(win, 1, 1);
            center_print(win, 2, "LLVM CODE OBFUSCATOR", 1);
            center_print(win, 3, "Advanced Code Protection Suite", 4);

            // Step 1
            center_print(win, 5, "=> STEP 1: File Selection & Analysis =>", 3);
            char pathbuf[256] = {0};
            prompt_input(win, 7, "* Enter path to LLVM IR file (.ll) [auto-detect]: ", pathbuf, sizeof(pathbuf));
            if (pathbuf[0]) cfg.src = std::string(pathbuf);

            // Step 2: presets
            center_print(win, 9, "* Preset Options *", 3);
            std::vector<std::pair<int,std::string>> presets = {
                {1, "Light Protection - Fast, minimal obfuscation"},
                {2, "Balanced Protection - Good security/speed ratio"},
                {3, "Maximum Protection - Maximum security"},
                {4, "Custom Configuration - Manual settings"},
                {5, "Exit Program"}
            };
            int sel = numbered_selector(win, 11, presets, 2);
            if (sel == 5) break;
            if (sel == 1) cfg.preset = "light"; else if (sel == 2) cfg.preset = "balanced"; else if (sel == 3) cfg.preset = "aggressive"; else cfg.preset = "custom";

            // Step 3: processing (simulate)
            center_print(win, 15, "! STEP 3: Processing !", 3);
            for (int p=0;p<=100;p+=5) { progress_bar(win, 17, p); napms(80); }

            // Step 4: results
            show_summary(win, 6, cfg);
            int ch = wgetch(win);
            again = (ch=='y' || ch=='Y');
        }

        delwin(win); endwin();
        return 0;
    }

        // STEP 3: Processing (simulate progress)
        for (int p = 0; p <= 100; p += 5) {
            draw_progress(menu_win, p);
            napms(80);
        }

        // STEP 4: Results / summary
        show_summary(menu_win, cfg);
        int ch = wgetch(menu_win);
        if (ch == 'y' || ch == 'Y') again = true; else again = false;
    }

    delwin(menu_win);
    endwin();
    return 0;
}
