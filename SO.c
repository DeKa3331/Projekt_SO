#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define TOTAL_CARDS 24

enum badge {HEART, SPADE, DIAMOND, CLUB}; // Kier, Pik, Karo, Trefl

enum suit {NINE = 9, TEN = 10, JACK = 11, QUEEN = 12, KING = 13, ACE = 14};

typedef struct {
    int suit;
    int badge;
} Card;

typedef struct {
    int player_id;
    int cards_cout;
    Card cards[TOTAL_CARDS];
    int current_card;
} Player;


pthread_mutex_t lock;
int current_value = 0;

void *play_game(void *arg) {
    Player *player = (Player *)arg;
    Card card_played = {-1, -1};

    pthread_mutex_lock(&lock);
    for (int i = 0; i < player->cards_cout; i++) {
        if (player->cards[i].suit >= current_value) {
            card_played = player->cards[i];
            player->cards[i].badge = -1;  // Mark card as played
            player->cards[i].suit = -1;
            current_value = card_played.suit;
            printf("Player %d plays: %d\n", player->player_id, card_played);
            break;
        }
    }
    pthread_mutex_unlock(&lock);

    if (card_played.badge == -1 && card_played.suit == -1) {
        printf("Player %d cannot play any card.\n", player->player_id);
    }

    return NULL;
}

void shuffle_deck(Card *deck) {
    for (int i = TOTAL_CARDS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card temp = deck[i];
        deck[i] = deck[j];
        deck[j] = temp;
    }
}

void fill_deck(Card *deck) {
    int index = 0;
    for(int suit=NINE; suit<=ACE; suit++) {
        for(int badge=HEART; badge<=CLUB; badge++) {
            deck[index].badge = badge;
            deck[index++].suit = suit;
        }
    }
}

void players_init(Player *players, Card *deck, int *num_players) {
    for (int i = 0; i < *num_players; i++) {
        players[i].player_id = i + 1;
        for (int j = 0; j < TOTAL_CARDS / *num_players; j++) {
            players[i].cards[j] = deck[i * TOTAL_CARDS / *num_players + j];
        }
    }
}

int main() {
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

    srand(time(NULL));

    pthread_t threads[num_players];
    Player players[num_players];
    players_init(players, deck, &num_players);


    pthread_mutex_init(&lock, NULL);

    int round = 0;
    while (round < TOTAL_CARDS / num_players) {
        for (int i = 0; i < num_players; i++) {
            pthread_create(&threads[i], NULL, play_game, &players[i]);
        }

        for (int i = 0; i < num_players; i++) {
            pthread_join(threads[i], NULL);
        }

        round++;
    }

    pthread_mutex_destroy(&lock);

    return 0;
}
