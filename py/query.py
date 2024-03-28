"""
# show JCR data with simple searching utility
TODO:
- add more data
- print complete list
"""

#config
data_folder="./data/"
data_file=data_folder+"JCR2022-UTF8.csv"


import csv
import pandas as pd
from fuzzywuzzy import fuzz 
from fuzzywuzzy import process
from simple_term_menu import TerminalMenu


#load data into proper format, prepare menu
def init():
    print("""
    ShowJCR, modified by Weilei Zeng, Mar 28, 2024
    """)
    df=pd.read_csv(data_file)
    col='IF(2022)'
    df=df.replace({'<0.1':'0'}) # get data ready for float conversoin
    df[col] = df[col].astype(float)
    js=df['Journal'].tolist() #for fuzzy search
    terminal_menu = TerminalMenu(
        ["[0] search", "[1] show full table", "[2] show table with IF>30"], 
        title="Show JCR menu. ENTER to select, q to quit"
    )

    return df,js,terminal_menu

import os
#print a page break
def page_break():
    cols, lins = os.get_terminal_size()
    print("~"*(cols-2))

        
def run(df,journal_list,terminal_menu):
    menu_entry_indice = terminal_menu.show()
#    menu_entry_indice = menu_entry_indices[0]
#    print(menu_entry_indices)
    print(menu_entry_indice)
    if menu_entry_indice==1:
        print(df)
    elif menu_entry_indice==2:
        d2=df.loc[ (df['IF(2022)'])>30]
        print(d2)
    elif menu_entry_indice==0:
        journal=input('enter the journal name...')
        print(journal)
        r=process.extract(journal, journal_list, limit=10)
        targets=[_[0] for _ in r ]
#        page_break()
        print(df.loc[ df['Journal'].isin( targets) ])
        page_break()
    else: #quit. also use escape or q to quit
        return 0
    return 1



def main():
    df,journal_list,terminal_menu=init()
    while(run(df,journal_list,terminal_menu)):
        pass
    print('Thanks for using the program. Goodbye!')

main()
    
