#include <stdio.h>
#include <ncurses/curses.h>
#include <string.h>
#include <stdlib.h>  // added for exit() function
#include <stdbool.h> // added for exit() function

void fail(char *msg) {
  endwin();
  puts(msg);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  /* Commandline argument currently unused */
  (void)argc;
  (void)argv;

  int i, c;
  (void)c; // c is currently unused
  char powitanie[] = "SLOWNIK UNIWERSALNY";
  int szer, dlug; // wartosci dlugosci i szerokosci terminalu

  initscr(); // Inizjalizacja całości ncurses, kolory itp

  raw();
  noecho();
  //  keypad(stdscr, TRUE);
  /* Test to see if terminal has colors */
  if (has_colors() == false) {
    fail("Colors unavailable\n");
  }

  if (start_color() != OK) {
    fail("Unable to start colors\n");
  }

  // init_pair(1, COLOR_BLUE, COLOR_BLACK); //wybór kolorów

  getmaxyx(stdscr, szer, dlug);                   // pobranie rozmiarów terminalu
  move(szer / 2, (dlug - strlen(powitanie)) / 2); // przesuwamy kursor na środek (tak aby się ładnie wydrukowało)
  // attron(COLOR_PAIR(1)); //Aktywujemy wybrane kolory
  printw(powitanie); // Drukujemy powitanie
  // attroff(COLOR_PAIR(1));//Dezaktywujemy kolory
  refresh();                                           // Odswiezamy (inaczej się nie wyswietli)
  WINDOW *menuwin = newwin(7, dlug - 12, szer - 9, 6); // Definiujemy i tworzymy 'okno'
  box(menuwin, ACS_VLINE, ACS_HLINE);
  refresh();             // ponownie odświeżamy aby okno się pojawiło
  wrefresh(menuwin);     // odświeżamy samo okno
  keypad(menuwin, TRUE); // umozliwiamy dzialanie klawiatury w oknie

  const char *opcje[] = {
      "Tlumacz z Polskiego na Angielski",
      "Tlumacz z Angielskiego na Polski",
      "Edystuj slownik",
      "Wybierz slownik",
      "Wyjdz",
  };
  int wybor;
  int zaznacz = 0;

  while (1) // cala ta petla sluzy ciaglemu tworzeniu menu z podswietleniem wybranego elementu
  {
    for (i = 0; i < 5; i++) {
      if (i == zaznacz)
        wattron(menuwin, A_REVERSE);
      mvwprintw(menuwin, i + 1, 1, opcje[i]);
      if (i == zaznacz)
        wattroff(menuwin, A_REVERSE);
    }

    wybor = wgetch(menuwin);
    switch (wybor) {
    case KEY_UP:
      zaznacz--;
      if (zaznacz < 0)
        zaznacz = 0; // zabezpieczenie przed wyjsciem "poza" menu
      break;
    case KEY_DOWN:
      zaznacz++;
      if (zaznacz > 4)
        zaznacz = 4;
      break;
    default:
      break;
    }

    if (wybor == 10)
      break;
  }

  printw("\nWybrano:%s", opcje[zaznacz]);
  refresh();

  /* Wait for user to press enter to exit */
  getch();

  /* Need to cleanup before exit */
  endwin();

  return 0;
}
