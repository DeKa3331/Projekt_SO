#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define TOTAL_CARDS 24
#define START_CARD {NINE, HEART}

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
    int current_card;
    int skipped; // used for 1st round to start from player with K9
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
void sort_by_suit(Card*, int);

pthread_mutex_t lock;

int is_game_done = 0;
int winner = -1;

int cards_played = 0;
Card card_pile[TOTAL_CARDS];

void add_to_pile(Player *player, int index) {
    card_pile[cards_played++] = player->cards[index];
    player->cards[index].suit = -1;
    player->cards[index].badge = -1;
    player->cards_out--;
}

void remove_from_pile(Player *player, int amount) {
    for(int i = 0; i < amount; i++) {
        for(int j = 0; j < TOTAL_CARDS; j++) {
            if(!(player->cards[j].badge == -1 && player->cards[j].suit == -1)) continue;
            player->cards[j] = card_pile[cards_played-- - 1];
            player->cards_out++;
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
    sort_by_suit(deck, card_amount);

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
    for(int i=0; i<card_amount; i++) {
        for(int j=0; j<i; j++) {
            if(deck[j].suit > deck[j+1].suit) swap(&deck[j], &deck[j+1]);
        }
    }
}

int card_playing_logic(Player *player) {  // Choosing what to play in hierarchy top -> bottom
    int index;

    // Looking for START_CARD (once)
    Card to_find = START_CARD;
    index = find_card(&to_find, player->cards, TOTAL_CARDS);
    if(cards_played == 0 && index != -1) {
        printf("Initialization!\n");
        printf("Player %d plays: ", player->player_id);
        print_card(&player->cards[index]);
        add_to_pile(player, index);
        printf(" Left: %d \n", player->cards_out);
        printf("\n");
        return 1;
    } else {
        player->skipped = 1;
    }

    // Looking for 3 or more of the same suit
    int card_amount;
    Card card_multiplied;
    have_same_cards(player->cards, TOTAL_CARDS, &card_multiplied, &card_amount);
    if(cards_played > 0 && card_amount >= 3) {
        printf("Three or more!\n");
        printf("Player %d plays: ", player->player_id);
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

    // Looking for smallest card to play
    index = find_smallest_card(player);
    if(cards_played > 0 && index != -1) {
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

    int is_card_played = 0;

    is_card_played = card_playing_logic(player);

    pthread_mutex_lock(&lock); // Lock mutex before modifying shared variables

    // Check if the player has finished their cards
    if(player->cards_out == 0) {
        is_game_done = 1;
        winner = player->player_id;
        pthread_mutex_unlock(&lock); // Unlock mutex before returning
        return NULL;
    }

    if (!is_card_played) {
        // Logic for handling when no card is played
        remove_from_pile(player, cards_played <= 3 ? cards_played - 1 : 3);
        printf("Player %d cannot play any card. Drawing 3\n", player->player_id);
        printf("Cards left: %d\n",player->cards_out);
        sort_by_suit(player->cards, TOTAL_CARDS);
        print_player_deck(player);
        printf("\n");
        if (cards_played == 0) {
            player->skipped = 1;
        }
    }

    pthread_mutex_unlock(&lock); // Unlock mutex after modifying shared variables

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
        players[i].cards_out = TOTAL_CARDS / *num_players;
        players[i].skipped = 0;
        for (int j = 0; j < TOTAL_CARDS / *num_players; j++) {
            players[i].cards[j] = deck[i * TOTAL_CARDS / *num_players + j];
        }
        for(int j = TOTAL_CARDS / *num_players; j < TOTAL_CARDS; j++) {
            players[i].cards[j].badge = -1;
            players[i].cards[j].suit = -1;
        }
    }
}

void print_deck(Card *deck, int size) {
    for(int i = 0; i < size; i++) {
        print_card(&deck[i]);
        printf(" ");
    }
    printf("\n");
}

void print_card(Card *card) {
    char suit, badge;
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
        case -1:
            suit = '0';
            break;
        default:
            break;
    }
    switch (card->badge) {
        case DIAMOND:
            badge = 'D';
            break;
        case HEART:
            badge = 'H';
            break;
        case SPADE:
            badge = 'S';
            break;
        case CLUB:
            badge = 'C';
            break;
        case -1:
            badge = '0';
            break;
        default:
            break;
    }
    printf("%c-%c", suit, badge);
}

void print_player_deck(Player *player) {
    for(int i = 0; i < TOTAL_CARDS; i++) {
        if(player->cards[i].suit != -1 && player->cards[i].badge != -1) {
            print_card(&player->cards[i]);
            printf(" ");
        }
    }
    printf("\n");
}

int find_first_player(Player *players, Card *card_to_find, int *num_players) {
    for(int i=0; i < *num_players; i++) {
        int index = find_card(card_to_find, players->cards, TOTAL_CARDS);
        if (index != -1) return index;
    }
}

int find_card(Card *card_to_find, Card *card_deck, int size) {
    for(int i=0; i<size; i++) {
        if(card_deck[i].badge == card_to_find->badge && card_deck[i].suit == card_to_find->suit) return i;
    }
    return -1;
}

int main() {
    srand(0);
    int num_players;
    printf("Enter the number of players: ");
    scanf("%d", &num_players);

    if (num_players <= 0 || num_players > 4) {
        printf("Number of players must be between 1 and 4.\n");
        return 1;
    }

    Card deck[TOTAL_CARDS];
    fill_deck(deck);
    shuffle_deck(deck);
    print_deck(deck, TOTAL_CARDS);

    pthread_t threads[num_players];
    Player players[num_players];
    players_init(players, deck, &num_players);

    pthread_mutex_init(&lock, NULL);

    int round = 0;
    Card to_find = START_CARD;
    int start_player = find_first_player(players, &to_find, &num_players);

while (!is_game_done) {
    // Existing loop logic...

    for (int i = 0; i < num_players; i++) {
        int current_player = (start_player + i) % num_players;
        pthread_create(&threads[current_player], NULL, play_game, &players[current_player]);
    }

    for (int i = 0; i < num_players; i++) {
        int current_player = (start_player + i) % num_players;
        pthread_join(threads[current_player], NULL);
    }

    if (is_game_done) {
        break; // Exit the loop if game is done
    }

    round++;
}

    printf("Player %d wins after %d rounds!\n", winner, round);
    pthread_mutex_destroy(&lock);

    return 0;
}
