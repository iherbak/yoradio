#ifndef utf8RusLCD_h
#define  utf8RusLCD_h


#ifdef HUN_LCD



char* DspCore::utf8Rus(const char* str, bool uppercase) {

  int index = 0;
  static char strn[BUFLEN];
  static char newStr[BUFLEN];
  bool E = false;
  strlcpy(strn, str, BUFLEN);
  newStr[0] = '\0';
  bool next = false;

  while (*str) {
    byte charIndex = 255;
    if ((*str & 0xE0) == 0xC0 || (*str & 0xF0) == 0xE0 || (*str & 0xF8) == 0xF0) {
      // UTF-8 karakterek kezelése
      uint16_t code = 0;
      if ((*str & 0xE0) == 0xC0) { // 2 bájtos UTF-8
        code = ((*str & 0x1F) << 6) | (*(str + 1) & 0x3F);
        str += 2;
      } else if ((*str & 0xF0) == 0xE0) { // 3 bájtos UTF-8
        code = ((*str & 0x0F) << 12) | ((*(str + 1) & 0x3F) << 6) | (*(str + 2) & 0x3F);
        str += 3;
      } else if ((*str & 0xF8) == 0xF0) { // 4 bájtos UTF-8
        code = ((*str & 0x07) << 18) | ((*(str + 1) & 0x3F) << 12) | ((*(str + 2) & 0x3F) << 6) | (*(str + 3) & 0x3F);
        str += 4;
      }

      switch (code) {
        case 0x00C1: charIndex = 1; break; // Á
        case 0x00E1: charIndex = 1; break; // á
        case 0x00C9: charIndex = 2; break; // É
        case 0x00E9: charIndex = 2; break; // é
        case 0x00CD: newStr[index++] = 'i';  continue; // Í
        case 0x00ED: newStr[index++] = 'I';  continue; // í
        case 0x00D3: charIndex = 3; break; // Ó
        case 0x00F3: charIndex = 3; break; // ó
        case 0x00D6: charIndex = 4; break; // Ö
        case 0x00F6: charIndex = 4; break; // ö
        case 0x0150: charIndex = 5; break; // Ő
        case 0x0151: charIndex = 5; break; // ő
        case 0x00DA: charIndex = 6; break; // Ú
        case 0x00FA: charIndex = 6; break; // ú
        case 0x00DC: charIndex = 7; break; // Ü
        case 0x00FC: charIndex = 7; break; // ü
        case 0x0170: charIndex = 7; break; // Ű
        case 0x0171: charIndex = 7; break; // ű
        default: newStr[index++] ='?'; continue; // Ismeretlen karakter helyettesítése kérdőjellel
      }
    }

    if (charIndex != 255) {
      newStr[index++] = charIndex;
    }
    else {
      newStr[index++] = *str; // Normál ASCII karakterek kezelése
      str++;
    }
  } //end while
  newStr[index] = 0;
  return newStr;
}

#else  //original RUS code
char* DspCore::utf8Rus(const char* str, bool uppercase) {
  int index = 0;
  static char strn[BUFLEN];
  static char newStr[BUFLEN];
  bool E = false;
  strlcpy(strn, str, BUFLEN);
  newStr[0] = '\0';
  bool next = false;
  for (char *iter = strn; *iter != '\0'; ++iter)
  {
    if (E) {
      E = false;
      continue;
    }
    byte rus = (byte) * iter;
    if (rus == 208 && (byte) * (iter + 1) == 129) { // ёКостыли
      *iter = (char)209;
      *(iter + 1) = (char)145;
      E = true;
      continue;
    }
    if (rus == 209 && (byte) * (iter + 1) == 145) {
      *iter = (char)209;
      *(iter + 1) = (char)145;
      E = true;
      continue;
    }
    if (next) {
      if (rus >= 128 && rus <= 143) *iter = (char)(rus + 32);
      if (rus >= 176 && rus <= 191) *iter = (char)(rus - 32);
      next = false;
    }
    if (rus == 208) next = true;
    if (rus == 209) {
      *iter = (char)208;
      next = true;
    }
    *iter = toupper(*iter);
  }

  while (strn[index])
  {
    if (strlen(newStr) > BUFLEN - 2) break;
    if (strn[index] >= 0xBF)
    {
      switch (strn[index]) {
        case 0xD0: {
            switch (strn[index + 1])
            {
              case 0x90: strcat(newStr, "A"); break;
              case 0x91: strcat(newStr, "B"); break;
              case 0x92: strcat(newStr, "V"); break;
              case 0x93: strcat(newStr, "G"); break;
              case 0x94: strcat(newStr, "D"); break;
              case 0x95: strcat(newStr, "E"); break;
              case 0x96: strcat(newStr, "ZH"); break;
              case 0x97: strcat(newStr, "Z"); break;
              case 0x98: strcat(newStr, "I"); break;
              case 0x99: strcat(newStr, "Y"); break;
              case 0x9A: strcat(newStr, "K"); break;
              case 0x9B: strcat(newStr, "L"); break;
              case 0x9C: strcat(newStr, "M"); break;
              case 0x9D: strcat(newStr, "N"); break;
              case 0x9E: strcat(newStr, "O"); break;
              case 0x9F: strcat(newStr, "P"); break;
              case 0xA0: strcat(newStr, "R"); break;
              case 0xA1: strcat(newStr, "S"); break;
              case 0xA2: strcat(newStr, "T"); break;
              case 0xA3: strcat(newStr, "U"); break;
              case 0xA4: strcat(newStr, "F"); break;
              case 0xA5: strcat(newStr, "H"); break;
              case 0xA6: strcat(newStr, "TS"); break;
              case 0xA7: strcat(newStr, "CH"); break;
              case 0xA8: strcat(newStr, "SH"); break;
              case 0xA9: strcat(newStr, "SHCH"); break;
              case 0xAA: strcat(newStr, "'"); break;
              case 0xAB: strcat(newStr, "YU"); break;
              case 0xAC: strcat(newStr, "'"); break;
              case 0xAD: strcat(newStr, "E"); break;
              case 0xAE: strcat(newStr, "YU"); break;
              case 0xAF: strcat(newStr, "YA"); break;
            }
            break;
          }
        case 0xD1: {
            if (strn[index + 1] == 0x91) {
              strcat(newStr, "YO"); break;
              break;
            }
            break;
          }
      }
      int sind = index + 2;
      while (strn[sind]) {
        strn[sind - 1] = strn[sind];
        sind++;
      }
      strn[sind - 1] = 0;
    } else {
    	if(strn[index]==7) strn[index]=165;
    	if(strn[index]==9) strn[index]=223;
      char Temp[2] = {(char) strn[index] , 0 } ;
      strcat(newStr, Temp);
    }
    index++;
  }
  return newStr;
}
#endif

#endif
