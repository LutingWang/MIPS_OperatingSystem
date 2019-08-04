"""""""""""
" Plugins "
"""""""""""
filetype plugin indent on
let g:NERDTreeWinSize=20
" let g:nerdtree_tabs_open_on_console_startup=1
let Tlist_File_Fold_Auto_Close=1
let Tlist_Exit_OnlyWindow=1
let Tlist_Ctags_Cmd="/usr/bin/ctags"
let Tlist_Auto_Open=1
let Tlist_Use_Right_Window=1
let Tlist_WinWidth=30

""""""""""
" basics "
""""""""""
set nocompatible
set tags=~/tags;tags;/
set nu
set showcmd
set ruler
set scrolloff=3
set mouse=a
set foldenable
set foldmethod=syntax
set t_Co=256

"""""""""""""""
" key mapping "
"""""""""""""""
inoremap <TAB> <c-r>=SkipPair()<CR>

map <C-A> ggVGY
map! <C-A> <Esc>ggVGY
vmap <C-c> "+y

map <F5> :call CompileRunGcc()<CR>
map <F8> :call Rungdb()<CR>
map <F9> gg=G
" map <F10> :NERDTreeToggle<CR>
map <F10> <plug>NERDTreeTabsToggle <CR>
map <F11> :TlistToggle<CR>
map <F12> :shell<CR>

"""""""""""""""""""""""""""""""""
" Switch syntax highlighting on "
"""""""""""""""""""""""""""""""""
syntax on
autocmd BufRead,BufNewFile *.md set filetype=markdown
let g:molokai_original = 1
highlight NonText guibg=#060606
highlight Folded  guibg=#0A0A0A guifg=#9090D0
highlight Search ctermbg=220 ctermfg=Black
highlight Statement ctermfg=39
highlight LineNr ctermfg=Green
highlight PmenuSel ctermfg=Black

""""""""""""""""""""""""""
" Highlight current line "
""""""""""""""""""""""""""
set cursorcolumn
set cursorline
highlight CursorLine cterm=NONE ctermbg=30 ctermfg=NONE guibg=NONE guifg=NONE
highlight CursorColumn cterm=NONE ctermbg=30 ctermfg=NONE guibg=NONE guifg=NONE

""""""""""
" helper "
""""""""""
set linespace=2
set whichwrap=b,s,<,>,[,]
set wildmenu

set showmatch
set selection=exclusive
set selectmode=mouse,key

set laststatus=1

set gdefault
set encoding=utf-8
set fileencodings=utf-8,ucs-bom,shift-jis,gb18030,gbk,gb2312,cp936,utf-16,big5,e

set hlsearch
set incsearch
set ignorecase

set clipboard+=unnamed

""""""""""
" indent "
""""""""""
set autoindent
set cindent
set shiftwidth=4
set softtabstop=4
set tabstop=4

""""""""""""
" complete "
""""""""""""
set completeopt=longest,menu
inoremap ' ''<ESC>i
inoremap " ""<ESC>i
inoremap < <><ESC>i
inoremap ( ()<ESC>i
inoremap [ []<ESC>i
inoremap { {<CR>}<ESC>O

"""""""""""""
" functions "
"""""""""""""
func SkipPair()
        if getline('.')[col('.') - 1] == ')' || getline('.')[col('.') - 1] == ']== '"' || getline('.')[col('.') - 1] == "'" || getline('.')[col('.') - 1] == '}'
                return "\<ESC>la"
        else
                return "\t"
        endif
endfunc

autocmd BufNewFile *.cpp,*.[ch],*.sh,*.java exec ":call SetTitle()"
func SetTitle()
        if &filetype == 'sh'
                call setline(1,"\###############################################")
                call append(line("."), "\# File Name: ".expand("%"))
                call append(line(".")+1, "\# Author: Luting Wang")
                call append(line(".")+2, "\# mail: 2457348692@qq.com")
                call append(line(".")+3, "\# Created Time: ".strftime("%c"))
                call append(line(".")+4, "\#########################################")
                call append(line(".")+5, "\#!/bin/bash")
                call append(line(".")+6, "")
        else
                call setline(1, "/**********************************************")
                call append(line("."), "    > File Name: ".expand("%"))
                call append(line(".")+1, "    > Author: Luting Wang")
                call append(line(".")+2, "    > Mail: 2457348692@qq.com ")
                call append(line(".")+3, "    > Created Time: ".strftime("%c"))
                call append(line(".")+4, " ****************************************/")
                call append(line(".")+5, "")
        endif
        if &filetype == 'cpp'
                call append(line(".")+6, "#include<iostream>")
                call append(line(".")+7, "using namespace std;")
                call append(line(".")+8, "")
        endif
        if &filetype == 'c'
                call append(line(".")+6, "#include<stdio.h>")
                call append(line(".")+7, "")
        endif
        " append after creating new file
        autocmd BufNewFile * normal G
endfunc

" C/C++ run
func! CompileRunGcc()
        exec "w"
        if &filetype == 'c'
                exec "!g++ % -o %<"
                exec "! ./%<"
        elseif &filetype == 'cpp'
                exec "!g++ % -o %<"
                exec "! ./%<"
        elseif &filetype == 'java'
                exec "!javac %"
                exec "!java %<"
        elseif &filetype == 'sh'
                :!./%
        endif
endfunc

" C/C++ debug
func! Rungdb()
        exec "w"
        exec "!g++ % -g -o %<"
        exec "!gdb ./%<"
endfunc
