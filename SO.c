#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define TOTAL_CARDS 24
#define START_CARD {NINE, HEART}

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define RESET "\x1B[0m"

enum badge {HEART, SPADE, DIAMOND, CLUB}; // Kier, Pik, Karo, Trefl
enum suit {NINE = 9, TEN = 10, JACK = 11, QUEEN = 12, KING = 13, ACE = 14};

typedef struct {
    int suit;
    int badge;
} Card;

typedef struct {
    int player_id;
    int cards_out;
    Card cards[TOTAL_CARDS];
    int next_player_id;
    int prev_player_id;
    int rounds_played;
    int cards_played;
    int cards_drawed;
} Player;

void *play_game(void*);
void shuffle_deck(Card*);
void fill_deck(Card*);
void players_init(Player*, Card*, int*);
void print_deck(Card *deck, int);
void print_card(Card*);
void swap(Card*, Card*);
void print_player_deck(Player* player);
int find_card(Card*, Card*, int);
void sort_by_suit(Card[], int);

pthread_mutex_t lock;

int is_game_done = 0;
int winner = -1;
int actual_player = 0;
int cards_played = 0;
Card card_pile[TOTAL_CARDS];

void add_to_pile(Player *player, int index) {
    card_pile[cards_played++] = player->cards[index];
    player->cards[index].suit = -1;
    player->cards[index].badge = -1;
    player->cards_out--;
    player->cards_played++;
}

void remove_from_pile(Player *player, int amount) {
    for(int i = 0; i < amount; i++) {
        for(int j = 0; j < TOTAL_CARDS; j++) {
            if(!(player->cards[j].badge == -1 && player->cards[j].suit == -1)) continue;
            player->cards[j] = card_pile[cards_played-- - 1];
            player->cards_out++;
            player->cards_drawed++;
            break;
        }
    }
}

int find_smallest_card(Player *player) {
    int min_index = -1;
    for (int i = 0; i < TOTAL_CARDS; i++) {
        if (player->cards[i].suit >= card_pile[cards_played-1].suit) {
            if (min_index == -1 || player->cards[i].suit < player->cards[min_index].suit) {
                min_index = i;
            }
        }
    }
    return min_index;
}

void have_same_cards(Card deck[], int card_amount, Card *copies, int *copy_amount) {
    *copy_amount = 0;

    int counter = 0;
    Card copies_tmp = {-1, -1};
    for(int i=0; i<card_amount; i++) {
        if(deck[i].suit == -1) continue;

        if(deck[i].suit != copies_tmp.suit) {
            if(counter > *copy_amount) {
                *copy_amount = counter;
                *copies = copies_tmp;
            }
            copies_tmp = deck[i];
            counter = 1;
        } else {
            counter++;
        }
    }
}

void sort_by_suit(Card deck[], int card_amount) {
    for(int i=0; i<card_amount - 1; i++) {
        for(int j=0; j<card_amount - i - 1; j++) {
            if(deck[j].suit > deck[j+1].suit) swap(&deck[j], &deck[j+1]);
        }
    }
}

int card_playing_logic(Player *player) {  // Choosing what to play in hierarchy top -> bottom
    sort_by_suit(player->cards, TOTAL_CARDS);
    int index;

    // Looking for START_CARD (once)
    Card to_find = START_CARD;
    index = find_card(&to_find, player->cards, TOTAL_CARDS);
    if(cards_played == 0 && index != -1 && is_game_done != 1) {
        printf("\nInitialization!\n");
        printf("Player %d plays: ", player->player_id);
        print_card(&player->cards[index]);
        add_to_pile(player, index);
        printf(" Left: %d \n", player->cards_out);
        printf("\n");
        return 1;
    }

    // Looking for 3 or more of the same suit
    int card_amount;
    Card card_multiplied;
    have_same_cards(player->cards, TOTAL_CARDS, &card_multiplied, &card_amount);
    if(cards_played > 0 && card_amount >= 3 && is_game_done != 1) {
        printf("Three or more!\n");
        printf("Player %d plays: ", player->player_id);
        sort_by_suit(player->cards, TOTAL_CARDS);
        index = find_card(&card_multiplied, player->cards, TOTAL_CARDS);

        for(int i=0; i < card_amount; i++) {
            print_card(&player->cards[index+i]);
            printf(" ");
            add_to_pile(player, index+i);
        }
        printf(" Left: %d \n", player->cards_out);
        print_player_deck(player);
        printf("\n");
        return 1;
    }

    // Random chance to play random card (if not then continue to smallest)
    const int RANDOM_TRIES = 10;
    for(int i=0; i<RANDOM_TRIES; i++) {
        if(rand() % 10 != 0) break;
        index = rand() % TOTAL_CARDS;
        if(player->cards[index].suit == -1 && player->cards[index].badge == -1) {
            i--;
            continue;
        }
        if(cards_played > 1 && !is_game_done && player->cards[index].suit >= card_pile[cards_played-1].suit) {
            printf("Random card!\n");
            printf("Player %d plays: ", player->player_id);
            print_card(&player->cards[index]);
            add_to_pile(player, index);
            printf(" Left: %d \n", player->cards_out);
            print_player_deck(player);
            printf("\n");
            return 1;
        }
    }

    // Looking for smallest card to play
    index = find_smallest_card(player);
    if(cards_played > 0 && index != -1 && is_game_done != 1) {
        printf("Small card!\n");
        printf("Player %d plays: ", player->player_id);
        print_card(&player->cards[index]);
        add_to_pile(player, index);
        printf(" Left: %d \n", player->cards_out);
        print_player_deck(player);
        printf("\n");
        return 1;
    }

    return 0;
}

void *play_game(void *arg) {
    Player *player = (Player *)arg;

    while(1) {
        int is_card_played = 0;

        while(player->player_id != actual_player && !is_game_done); // Stop players when other is playing
        if(is_game_done) return NULL;
        pthread_mutex_lock(&lock);

        is_card_played = card_playing_logic(player);
        player->rounds_played++;

        // Logic if played card is SPADE (pik)
        if(is_card_played && card_pile[cards_played - 1].badge == SPADE) {
            actual_player = player->prev_player_id;
        } else {
            actual_player = player->next_player_id;
        }

        // Logic for handling when no card is played
        if (!is_card_played && is_game_done != 1) {
            printf("Player %d cannot play any card. Drawing %d\n", player->player_id, cards_played <= 3 ? cards_played - 1 : 3);
            remove_from_pile(player, cards_played <= 3 ? cards_played - 1 : 3);
            printf("Cards left: %d\n", player->cards_out);
            sort_by_suit(player->cards, TOTAL_CARDS);
            print_player_deck(player);
            printf("\n");
        }
        
        // Check if the player has finished their cards
        if(player->cards_out == 0) {
            is_game_done = 1;
            winner = player->player_id;
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        

        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void swap(Card *c1, Card *c2) {
    Card tmp = *c1;
    *c1 = *c2;
    *c2 = tmp;
}

void shuffle_deck(Card *deck) {
    for(int j=0; j<7; j++) {
        for(int i = 0; i < TOTAL_CARDS; i++) {
            int index = rand() % TOTAL_CARDS;
            swap(&deck[i], &deck[index]);
        }
    }
}

void fill_deck(Card *deck) {
    int index = 0;
    for(int suit = NINE; suit <= ACE; suit++) {
        for(int badge = HEART; badge <= CLUB; badge++) {
            deck[index].badge = badge;
            deck[index++].suit = suit;
        }
    }
}

void players_init(Player *players, Card *deck, int *num_players) {
    for (int i = 0; i < *num_players; i++) {
        players[i].player_id = i + 1;

        if(i == *num_players - 1) players[i].next_player_id = 1;
        else players[i].next_player_id = i + 2;

        if(i == 0) players[i].prev_player_id = *num_players;
        else players[i].prev_player_id = i;

        players[i].cards_out = TOTAL_CARDS / *num_players;
        players[i].rounds_played = 0;
        players[i].cards_drawed = 0;
        players[i].cards_played = 0;
        for (int j = 0; j < TOTAL_CARDS / *num_players; j++) {
            players[i].cards[j] = deck[i * TOTAL_CARDS / *num_players + j];
        }
        for (int j = TOTAL_CARDS / *num_players; j < TOTAL_CARDS; j++) {
            players[i].cards[j].badge = -1;
            players[i].cards[j].suit = -1;
        }
           }
    }

void print_deck(Card *deck, int size) {
    for (int i = 0; i < size; i++) {
        print_card(&deck[i]);
        printf(" ");
    }
    printf("\n");
}

void print_card(Card *card) {
    char suit;
    char *color;
    switch (card->badge) {
        case HEART:
            color = RED;
            break;
        case DIAMOND:
            color = MAGENTA;
            break;
        case SPADE:
            color = BLUE;
            break;
        case CLUB:
            color = GREEN;
            break;
        default:
            color = RESET;
            break;
    }
    switch (card->suit) {
        case NINE:
            suit = '9';
            break;
        case TEN:
            suit = 'T';
            break;
        case JACK:
            suit = 'J';
            break;
        case QUEEN:
            suit = 'Q';
            break;
        case KING:
            suit = 'K';
            break;
        case ACE:
            suit = 'A';
            break;
        default:
            suit = '?';
            break;
    }

    printf("%s[ %c", color, suit);

    switch (card->badge) {
        case DIAMOND:
            printf("\u2666 ");
            break;
        case HEART:
            printf("\u2665 ");
            break;
        case SPADE:
            printf("\u2660 ");
            break;
        case CLUB:
            printf("\u2663 ");
            break;
        default:
            printf(" ? ");
            break;
    }
    printf("]%s", RESET);
}

void print_player_deck(Player *player) {
    printf("Player %d's cards: ", player->player_id);
    for (int i = 0; i < TOTAL_CARDS; i++) {
        if (player->cards[i].suit != -1 && player->cards[i].badge != -1) {
            print_card(&player->cards[i]);
            printf(" ");
        }
    }
    printf("\n");
}

int find_first_player(Player *players, Card *card_to_find, int *num_players) {
    for (int i = 0; i < *num_players; i++) {
        int index = find_card(card_to_find, players[i].cards, TOTAL_CARDS);
        if (index != -1) return players[i].player_id;
    }
    return -1;
}

int find_card(Card *card_to_find, Card *card_deck, int size) {
    for (int i = 0; i < size; i++) {
        if (card_deck[i].badge == card_to_find->badge && card_deck[i].suit == card_to_find->suit) return i;
    }
    return -1;
}

int main() {
    time_t t = time(NULL);
    srand(t);
    printf("Seed %ld\n", (long int)t);

    int num_players;
    printf("Enter the number of players (2-4): ");
    scanf("%d", &num_players);

    if (num_players < 2 || num_players > 4) {
        printf("Number of players must be between 2 and 4.\n");
        return 1;
    }

    int game_mode;
    printf("Choose game mode:\n");
    printf("1. Human vs. Bots\n");
    printf("2. Bots vs. Bots\n");
    printf("Enter your choice (1-2): ");
    scanf("%d", &game_mode);

    if (game_mode != 1 && game_mode != 2) {
        printf("Invalid choice. Exiting.\n");
        return 1;
    }

    Card deck[TOTAL_CARDS];
    fill_deck(deck);
    shuffle_deck(deck);
    print_deck(deck, TOTAL_CARDS);

    pthread_t threads[num_players];
    Player players[num_players - (game_mode == 1 ? 1 : 0)]; // Adjust number of players based on game mode
    int num_actual_players = num_players - (game_mode == 1 ? 1 : 0); // Store the actual number of players

    players_init(players, deck, &num_actual_players);

    pthread_mutex_init(&lock, NULL);

    int round = 0;
    Card to_find = START_CARD;
    int start_player = find_first_player(players, &to_find, &num_actual_players);
    actual_player = start_player;

    if (start_player == -1) {
        printf("No player has the starting card.\n");
        return 1;
    }

    // Thread initialization
    for(int i=0; i<num_actual_players; i++) {
        int current_player = (start_player + i) % num_actual_players;
        pthread_create(&threads[current_player], NULL, play_game, &players[current_player]);
    }
    
    for(int i=0; i<num_actual_players; i++) {
        int current_player = (start_player + 1) % num_actual_players;
        pthread_join(threads[current_player], NULL);
    }

    // Informations
    int total_cards_played = 0;
    int total_cards_drawed = 0;
    for(int i=0; i<num_actual_players; i++) {
        printf("Player %d played %d times with total of %d cards played and %d cards drawed.\n", 
            players[i].player_id,
            players[i].rounds_played,
            players[i].cards_played,
            players[i].cards_drawed);
        total_cards_played += players[i].cards_played;
        total_cards_drawed += players[i].cards_drawed;
    }
    printf("Total cards played: %d\n", total_cards_played);
    printf("Total cards drawed: %d\n\n", total_cards_drawed);

    // Determining places based (n the number of players
    int places[num_actual_players];
    for (int i = 0; i < num_actual_players; i++) {
        places[i] = 0;
    }

    // Determine the winner's position
    places[winner - 1] = 1;

    // Determine positions for the rest of the players
    int position = 2;
    for (int i = 0; i < num_actual_players; i++) {
        if (i != (winner - 1)) { // Skip the winner
            places[i] = position++;
        }
    }

    // Print results based on the number of players
    switch (num_actual_players) {
        case 2:
            printf("Player %d is the winner!\n", winner);
            break;
        case 3:
            for (int i = 0; i < num_actual_players; i++) {
                if (places[i] == 1) {
                    printf("Player %d is the winner!\n", i + 1);
                } else {
                    printf("Player %d is in %d place.\n", i + 1, places[i]);
                }
            }
            break;
        case 4:
            for (int i = 0; i < num_actual_players; i++) {
                if (places[i] == 1) {
                    printf("Player %d is the winner!\n", i + 1);
                } else if (places[i] == 2) {
                    printf("Player %d is in 2nd place.\n", i + 1);
                } else if (places[i] == 3) {
                    printf("Player %d is in 3rd place.\n", i + 1);
                } else {
                    printf("Player %d is in 4th place.\n", i + 1);
                }
            }
            break;
        default:
            break;
    }

    pthread_mutex_destroy(&lock);

    return 0;
}
