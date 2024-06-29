#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define TOTAL_CARDS 24
#define START_CARD {NINE, HEART}

#define PURPLE "\x1B[35m"
#define ORANGE "\x1B[38;5;208m"
#define LIGHT_BLUE "\x1B[96m"
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define RESET "\x1B[0m"
#define GOLD "\x1B[38;5;220m"
#define SILVER "\x1B[38;5;250m"
#define BRONZE "\x1B[38;5;136m"

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
    int active;
    int player_rank;
} Player;

//all functions predifined

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

//po co tutaj jest ten lock?????????
pthread_mutex_t lock;



//global variables
Player players[4];
int game_mode=0;
int num_players=0;
int is_game_done = 0;
int actual_player = 0;
int cards_played = 0;
int taken_places=0;
int active_players=0;
Card card_pile[TOTAL_CARDS];


//add played card to discard pile
void add_to_pile(Player *player, int index) {
    card_pile[cards_played++] = player->cards[index];
    player->cards[index].suit = -1;
    player->cards[index].badge = -1;
    player->cards_out--;
    player->cards_played++;
}
//remove played card to discard pile
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


//self explenatory
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


//checking for the same card (used for searching for 3 at the time rule)
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


//sorted by number -from 9 to Ace
void sort_by_suit(Card deck[], int card_amount) {
    for(int i=0; i<card_amount - 1; i++) {
        for(int j=0; j<card_amount - i - 1; j++) {
            if(deck[j].suit > deck[j+1].suit) swap(&deck[j], &deck[j+1]);
        }
    }
}


//all card logic that we created, how cards can be played

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

//control flow of the game
void *play_game(void *arg) {
    Player *player = (Player *)arg;
    while (1) {
        int is_card_played = 0;

        while (player->player_id != actual_player && !is_game_done); //prevent other player from playing when one is playing
        if (is_game_done) return NULL;

        //checking if player should get a mutex to play the game (still have cards)
        if (player->active == 1) {
            is_card_played = card_playing_logic(player);
        }

        pthread_mutex_lock(&lock);

        //playing game
        if (player->active == 1) {
            player->rounds_played++;

            //checking for spade which resoult in changing order
            if (is_card_played && card_pile[cards_played - 1].badge == SPADE) {
                actual_player = player->prev_player_id;
            } else {
                actual_player = player->next_player_id;
            }
            //solving players can't play vard'
            if (!is_card_played && is_game_done != 1) {
                printf("Player %d cannot play any card. Drawing %d\n", player->player_id, cards_played <= 3 ? cards_played - 1 : 3);
                remove_from_pile(player, cards_played <= 3 ? cards_played - 1 : 3);
                printf("Cards left: %d\n", player->cards_out);
                sort_by_suit(player->cards, TOTAL_CARDS);
                print_player_deck(player);
                printf("\n");
            }

            //checking ending game
            if (active_players <= 1) {
                is_game_done = 1;

                pthread_mutex_unlock(&lock);
                return NULL;
            }


            //checking if player is out of cards and setting up places
            if (player->cards_out <= 0) {
                player->active = 0;
                active_players--;
                printf("Player %d is out of cards.\n", player->player_id);
                player->player_rank = num_players - taken_places; // Assign rank based on order of finishing
                taken_places--;
}
            //skipping inactive
            while (players[actual_player - 1].active == 0) {
                actual_player = players[actual_player - 1].next_player_id;
            }


        }

        pthread_mutex_unlock(&lock);
    }
}


//temp fuction to swarp cards
void swap(Card *c1, Card *c2) {
    Card tmp = *c1;
    *c1 = *c2;
    *c2 = tmp;
}

//self explenatory
void shuffle_deck(Card *deck) {
    for(int j=0; j<7; j++) {
        for(int i = 0; i < TOTAL_CARDS; i++) {
            int index = rand() % TOTAL_CARDS;
            swap(&deck[i], &deck[index]);
        }
    }
}

//filling deck with card we want to use to play game (for us it's 9 to Ace)
void fill_deck(Card *deck) {
    int index = 0;
    for(int suit = NINE; suit <= ACE; suit++) {
        for(int badge = HEART; badge <= CLUB; badge++) {
            deck[index].badge = badge;
            deck[index++].suit = suit;
        }
    }
}

//inicjalization player rules and base variables
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
        players[i].active = 1;
        for (int j = 0; j < TOTAL_CARDS / *num_players; j++) {
            players[i].cards[j] = deck[i * TOTAL_CARDS / *num_players + j];
        }
        for (int j = TOTAL_CARDS / *num_players; j < TOTAL_CARDS; j++) {
            players[i].cards[j].badge = -1;
            players[i].cards[j].suit = -1;
        }
           }
    }

    //showing current deck
void print_deck(Card *deck, int size) {
    for (int i = 0; i < size; i++) {
        print_card(&deck[i]);
        printf(" ");
    }
    printf("\n");
}

    //priting specific card we want
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

//merge previous functions to print left cards on player
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

//used for inicjalization to start from 9K
int find_first_player(Player *players, Card *card_to_find, int *num_players) {
    for (int i = 0; i < *num_players; i++) {
        int index = find_card(card_to_find, players[i].cards, TOTAL_CARDS);
        if (index != -1) return players[i].player_id;
    }
    return -1;
}

//self explenatory for funtion above
int find_card(Card *card_to_find, Card *card_deck, int size) {
    for (int i = 0; i < size; i++) {
        if (card_deck[i].badge == card_to_find->badge && card_deck[i].suit == card_to_find->suit) return i;
    }
    return -1;
}

void initialize_game() {
    Card deck[TOTAL_CARDS];
    fill_deck(deck);
    shuffle_deck(deck);
    players_init(players, deck, &num_players);

    // Reset game state variables
    cards_played = 0;
    is_game_done = 0;
    actual_player = 0;

    // Clear card pile
    for (int i = 0; i < TOTAL_CARDS; i++) {
        card_pile[i].suit = -1;
        card_pile[i].badge = -1;
    }

    Card to_find = START_CARD;
    int start_player = find_first_player(players, &to_find, &num_players);
    actual_player = start_player;

    if (start_player == -1) {
        printf("No player has the starting card.\n");
    }
}

void create_new_game() {
    printf(CYAN"Enter the number of players (2-4): "RESET);
    scanf("%d", &num_players);
    taken_places = num_players - 1;
    active_players = num_players;

    if (num_players < 2 || num_players > 4) {
        printf(RED"Sorry, number of players must be between 2 and 4.\n"RESET);
        return;
    }

    printf(CYAN"Choose game mode:\n"RESET);
    printf(BLUE"1. Human vs. Bots\n"RESET);
    printf(BLUE"2. Bots vs. Bots\n"RESET);
    printf(BLUE"Enter your choice (1-2): " RESET);
    scanf("%d", &game_mode);

    if (game_mode != 1 && game_mode != 2) {
        printf("Invalid choice. Exiting.\n");
        return;
    }

    pthread_t threads[num_players];
    initialize_game(); // Call the function to initialize game state

    pthread_mutex_init(&lock, NULL);

    // Thread initialization
    for (int i = 0; i < num_players; i++) {
        pthread_create(&threads[i], NULL, play_game, &players[i]);
    }

    // Thread joining
    for (int i = 0; i < num_players; i++) {
        pthread_join(threads[i], NULL);
    }
}

    //endgame   Informations
void print_endgame() {
    printf(RED "\nGame Over!\n" RESET);
    printf("Player Rankings:\n");
      //fix for last player rank
    for (int i = 0; i < num_players; i++) {
            if(players[i].player_rank==0)
            {
                players[i].player_rank=num_players;//last rank
            }
    }
    //creating array to storage rankings
    Player ranking[num_players];
    for (int i = 0; i < num_players; i++) {
        ranking[players[i].player_rank - 1] = players[i];
    }
    for (int i = 0; i < num_players; i++) {
        if (ranking[i].player_rank == 1)
            printf(GOLD "%d place: Player %d\n" RESET, ranking[i].player_rank, ranking[i].player_id);
        else if (ranking[i].player_rank == 2)
            printf(SILVER "%d place: Player %d\n" RESET, ranking[i].player_rank, ranking[i].player_id);
        else if (ranking[i].player_rank == 3)
            printf(BRONZE "%d place: Player %d\n" RESET, ranking[i].player_rank, ranking[i].player_id);
        else
            printf("%d place: Player %d\n", ranking[i].player_rank, ranking[i].player_id);
    }
}


int main() {
    time_t t = time(NULL);
    srand(t);
    printf(YELLOW "Seed %ld\n", (long int)t);
    RESET;
    char choice;
    int choice2=2;
    int choice_number;

    create_new_game();
    print_endgame();


    do {

        while (1) {
            printf(LIGHT_BLUE"Do you want more detailed statistics or maybe play another game?\n");
            printf("Choose respective options:\n");
            printf("Y-for more info, N-to end game, A-to roll another game\n\n\n"RESET);
            scanf(" %c", &choice);

            switch (choice) {
                case 'Y':
                case 'y':
                int total_cards_played = 0;
                int total_cards_drawed = 0;
                printf(LIGHT_BLUE"Do you want statistics from one player or all?\n");
                printf("Choose respective number for each player, choose 5 for all\n"RESET);
                scanf("%d", &choice2);
                switch (choice2) {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                        choice_number = choice2 - 1;
                        printf(YELLOW "Player %d played %d times with total of %d cards played and %d cards drawn.\n",
                            players[choice_number].player_id,
                            players[choice_number].rounds_played,
                            players[choice_number].cards_played,
                            players[choice_number].cards_drawed);
                        printf(RESET);
                        break;
                    case 5:
                        for (int i = 0; i < num_players; i++) {
                            printf(YELLOW"Player %d played %d times with total of %d cards played and %d cards drawn.\n",
                                players[i].player_id,
                                players[i].rounds_played,
                                players[i].cards_played,
                                players[i].cards_drawed);
                            total_cards_played += players[i].cards_played;
                            total_cards_drawed += players[i].cards_drawed;
                            printf(RESET);
                        }
                        printf(PURPLE "Total cards played: %d\n", total_cards_played);
                        printf("Total cards drawn: %d\n\n", total_cards_drawed);
                        printf(RESET);
                        break;
                    default:
                        printf("Invalid choice.\n");
                }
                break;
                case 'N':
                case 'n':
                    printf(BLUE "Thanks For Playing!\n" RESET);
                    printf(BLUE "Hope you had fun trying out our minigame.\n" RESET);
                    printf(BLUE "Designed and developed by: DeKa and Emzeey\n" RESET);
                    pthread_mutex_destroy(&lock);
                    return 0;

                case 'A':
                case 'a':
                        create_new_game();
                        print_endgame();
                    break;

                default:
                    printf("Invalid choice.\n");
            }
        }
    } while (choice == 'A' || choice == 'a');

    pthread_mutex_destroy(&lock);
    return 0;
}
