# download latest amiibo data and merge to amiibo_data.csv


from urllib.request import urlopen
import json
import os
import csv

class I18nFile:
    def __init__(self):
        self.avaliable_languages = list()
        self.i18n_items = list()



def get_prorject_directory():
    return os.path.abspath(os.path.dirname(__file__)+"/../")


def read_i18n_from_csv():
    i18n = I18nFile()
    csv_file = get_prorject_directory() + "/data/i18n.csv"
    if not os.path.exists(csv_file):
        return i18n
   
    with open(csv_file, "r", encoding="utf8") as f:
        first_line = True
        for r in csv.reader(f):
            if first_line:
                first_line = False
                i18n.avaliable_languages = r[1:]
            else:
                i18n.i18n_items.append(r)
    return i18n

def write_i18n_string_id_file(i18n):
    c_file = get_prorject_directory() + "/application/src/i18n/string_id.h"
    with open(c_file, "w+", newline="\n", encoding="utf8") as f:
        f.write('#ifndef STRING_ID_H\n')
        f.write('#define STRING_ID_H\n')
        f.write('\n')
        f.write('typedef enum\n')
        f.write('{\n')
        for item in i18n.i18n_items:
            f.write('    '+item[0]+',\n')
        f.write('    _L_COUNT,\n')
        f.write('} L_StringID;\n')
        f.write('\n')
        f.write('#endif /* STRING_ID_H */\n')

def write_i18n_language_c(i18n, idx, lang):
    c_file = get_prorject_directory() + "/application/src/i18n/" + lang + ".c"
    with open(c_file, "w+", newline="\n", encoding="utf8") as f:
        f.write('#include "language.h"\n')
        f.write('\n')
        f.write('const char *const lang_'+lang+'[_L_COUNT] = {\n')
        for item in i18n.i18n_items:
            f.write('    [%s] = "%s",\n' % (item[0], item[idx]))
        f.write('};\n')


i18n_file = read_i18n_from_csv()
write_i18n_string_id_file(i18n_file)
idx = 1
for lang in i18n_file.avaliable_languages:
    write_i18n_language_c(i18n_file, idx, lang)
    idx += 1
