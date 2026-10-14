#pragma once

/**
 * @version	05/06/10 Bang2ni - Added item price queries (QID_ITEMPRICE_XXX)
 */
enum QID {
    QID_PLAYER,                         // 0
    QID_ITEM,                           // 1
    QID_QUEST,                          // 2
    QID_AFFECT,                         // 3
    QID_LOGIN,                          // 4
    QID_SAFEBOX_LOAD,                   // 5
    QID_SAFEBOX_CHANGE_SIZE,            // 6
    QID_SAFEBOX_CHANGE_PASSWORD,        // 7
    QID_SAFEBOX_CHANGE_PASSWORD_SECOND, // 8
    QID_SAFEBOX_SAVE,                   // 9
    QID_ITEM_SAVE,                      // 10
    QID_ITEM_DESTROY,                   // 11
    QID_QUEST_SAVE,                     // 12
    QID_PLAYER_SAVE,                    // 13
    QID_HIGHSCORE_REGISTER,             // 14
    QID_PLAYER_DELETE,                  // 15
    QID_LOGIN_BY_KEY,                   // 16
    QID_PLAYER_INDEX_CREATE,            // 17
    QID_ITEM_AWARD_LOAD,                // 18
    QID_ITEM_AWARD_TAKEN,               // 19
    QID_GUILD_RANKING,                  // 20

    // MYSHOP_PRICE_LIST
    QID_ITEMPRICE_SAVE,            ///< 21, item price save query
    QID_ITEMPRICE_DESTROY,         ///< 22, item price delete query
    QID_ITEMPRICE_LOAD_FOR_UPDATE, ///< 23, item price load query for price updates
    QID_ITEMPRICE_LOAD,            ///< 24, item price load query
    // END_OF_MYSHOP_PRICE_LIST
};
