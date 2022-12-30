#pragma once

// Compare two char* with length of 2
#define str2Equals(first, other) first[0] == other[0] && first[1] == other[1]

// Compare two char* with length of 3
#define str3Equals(first, other) str2Equals(first, other) && first[2] == other[2]

// Compare two char* with length of 4
#define str4Equals(first, other) str3Equals(first, other) && first[3] == other[3]

// Compare two char* with length of 5
#define str5Equals(first, other) str4Equals(first, other) && first[4] == other[4]

// Compare two char* with length of 6
#define str6Equals(first, other) str5Equals(first, other) && first[5] == other[5]

// Compare two char* with length of 7
#define str7Equals(first, other) str6Equals(first, other) && first[6] == other[6]

// Compare two char* with length of 8
#define str8Equals(first, other) str7Equals(first, other) && first[7] == other[7]

// Compare two char* with length of 9
#define str9Equals(first, other) str8Equals(first, other) && first[8] == other[8]

// Compare two char* with length of 10
#define str10Equals(first, other) str9Equals(first, other) && first[9] == other[9]