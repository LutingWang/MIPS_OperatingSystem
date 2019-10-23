"""""""""""
" Plugins "
"""""""""""
set nocompatible
filetype off
set rtp+=~/.vim/bundle/Vundle.vim
call vundle#begin()

Plugin 'VundleVim/Vundle.vim'
Plugin 'scrooloose/nerdtree'
Plugin 'jistr/vim-nerdtree-tabs'
Plugin 'vim-scripts/taglist.vim'
Plugin 'ludovicchabant/vim-gutentags'
Plugin 'octol/vim-cpp-enhanced-highlight'
Plugin 'Valloric/YouCompleteMe'

call vundle#end()
filetype plugin indent on

" nerdtree
let g:NERDTreeWinSize=20
" let g:nerdtree_tabs_open_on_console_startup=1

" taglist
set tags=~/.cache/tags/*.tags
let Tlist_File_Fold_Auto_Close=1
let Tlist_Exit_OnlyWindow=1
let Tlist_Ctags_Cmd="/usr/local/bin/ctags"
let Tlist_Auto_Open=1
let Tlist_Use_Right_Window=1
let Tlist_WinWidth=30

" gutentags
let g:gutentags_project_root=['.root', '.svn', '.git', '.project']
let g:gutentags_ctags_tagfile='.tags'
let g:gutentags_ctags_extra_args=['--fields=+niazS', '--extra=+q', '--c++-kinds=+pxI', '--c-kinds=+px']
let s:vim_tags = expand('~/.cache/tags')
let g:gutentags_cache_dir = s:vim_tags
if !isdirectory(s:vim_tags)
	silent! call mkdir(s:vim_tags, 'p')
endif

" vim-cpp-enhanced-highlight
let g:cpp_class_scope_highlight=1
let g:cpp_member_variable_highlight=1
let g:cpp_class_decl_highlight=1
let g:cpp_concepts_highlight=1
let g:cpp_experimental_simple_template_highlight=1

" YouCompleteMe
let g:ycm_global_ycm_extra_conf='~/.vim/bundle/YouCompleteMe/.ycm_extra_conf.py'
let g:ycm_key_list_select_completion=['<Down>']
let g:ycm_collect_indentifiers_from_tags_files=1
let g:ycm_seed_identifiers_with_syntax=1
let g:ycm_confirm_extra_conf=0
let g:ycm_autoclose_preview_window_after_completion=1
let g:ycm_complete_in_comments=1
let g:ycm_complete_in_strings=1
let g:ycm_collect_identifiers_from_comments_and_strings=1

""""""""""
" basics "
""""""""""
set number
set ruler
set cursorcolumn
set cursorline
set scrolloff=3
set showcmd
set laststatus=1
set showmatch " show matched parenthesis

set mouse=a
set whichwrap=<,>,[,] " allow cursor to move between lines
set foldenable
set foldmethod=syntax
set selectmode=mouse,key
set wildmenu " command line completion

set encoding=utf-8
set fileencodings=utf-8,ucs-bom,shift-jis,gb18030,gbk,gb2312,cp936,utf-16,big5,e
autocmd BufNewFile *.cpp,*.[ch],*.sh,*.java exec ":call SetTitle()"

"""""""""""""""
" key mapping "
"""""""""""""""
inoremap <TAB> <c-r>=SkipPair()<CR>
map <F5> :call CompileRunGcc()<CR>
map <F8> :call Rungdb()<CR>
" map <F10> :NERDTreeToggle<CR>
map <F10> <plug>NERDTreeTabsToggle <CR>
map <F11> :TlistToggle<CR>
map <F12> :shell<CR>

"""""""""""""
" highlight "
"""""""""""""
set t_Co=256
syntax on
hi SpecialKey		cterm=bold		ctermfg=81
hi Search					ctermfg=0	ctermbg=220
hi LineNr					ctermfg=Green
hi CursorLineNr		cterm=none		ctermfg=11
hi StatusLine		cterm=bold,reverse
hi StatusLineNC		cterm=reverse
hi Folded					ctermfg=14	ctermbg=242
hi FoldColumn					ctermfg=14	ctermbg=242
hi Pmenu					ctermfg=Black	ctermbg=White
hi PmenuSel					ctermfg=Black	ctermbg=Gray
hi CursorLine		cterm=none				ctermbg=242
hi CursorColumn							ctermbg=242
hi Comment					ctermfg=32
hi Identifier		cterm=bold		ctermfg=14
hi Statement					ctermfg=100
hi PreProc					ctermfg=5
hi Type						ctermfg=113

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
" inoremap < <><ESC>i
inoremap ( ()<ESC>i
inoremap [ []<ESC>i
inoremap { {<CR>}<ESC>O

"""""""""""""
" functions "
"""""""""""""
func SkipPair()
	if pumvisible()
		return "\<DOWN>\<ENTER>"
	elseif getline('.')[col('.') - 1] == ')' || getline('.')[col('.') - 1] == ']' || getline('.')[col('.') - 1] == '"' || getline('.')[col('.') - 1] == "'" || getline('.')[col('.') - 1] == '}'
		return "\<ESC>la"
    else
		return "\t"
    endif
endfunc

func SetTitle()
	if &filetype == 'sh'
		call setline(1,"\###############################################")
		call append(line("."), "\# File Name: ".expand("%"))
		call append(line(".")+1, "\# Author: Luting Wang")
		call append(line(".")+2, "\# mail: 2457348692@qq.com")
		call append(line(".")+3, "\# Created Time: ".strftime("%c"))
		call append(line(".")+4, "\###############################################")
		call append(line(".")+5, "\#!/bin/bash")
		call append(line(".")+6, "")
	else
		call setline(1, "/**********************************************")
		call append(line("."), "	> File Name: ".expand("%"))
		call append(line(".")+1, "	> Author: Luting Wang")
		call append(line(".")+2, "	> Mail: 2457348692@qq.com ")
		call append(line(".")+3, "	> Created Time: ".strftime("%c"))
		call append(line(".")+4, " **********************************************/")
		call append(line(".")+5, "")
	endif
	if &filetype == 'cpp'
		call append(line(".")+6, "#include <iostream>")
		call append(line(".")+7, "using namespace std;")
		call append(line(".")+8, "")
	elseif &filetype == 'c'
		call append(line(".")+6, "#include <stdio.h>")
		call append(line(".")+7, "")
	endif
	" append after creating new file
	autocmd BufNewFile * normal G
endfunc

" C/C++ run
func! CompileRunGcc()
	exec "w"
	if &filetype == 'c'
		exec "!g++ % -o %< -std=c++11"
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
	exec "!g++ % -g -o %< -std=c++11"
	exec "!gdb ./%<"
endfunc
