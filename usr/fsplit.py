import os


class FileParser:
    def __init__(self, f):
        self.f = f

    def readline(self):
        line = f.readline()
        if len(line) != 0 and line[-1] == '\n':
            line = line[:-1]
        if len(line) != 0 and line[-1] == '\r':
            line = line[:-1]
        return line


def parse_file(f):
    line = f.readline()
    while line:
        command, path = tuple(line.split())
        if command == '<!--DIR-->':
            print("in dir " + path + '\n')
            if not os.path.isdir(path):
                os.mkdir(path)
        elif command == '<!--FILE-->':
            print("in file " + path + '\n')
            with open(path, 'w') as file:
                line = f.readline()
                while not line == '<!--END-->':
                    file.write(line + '\n')
                    line = f.readline()
        else:
            print("unrecognized control sequence \"" + line + "\"\n")
        line = f.readline()


if __name__ == '__main__':
    with open("file_concat_output", 'r', encoding="UTF-8") as f:
        output_dir = "file_splitting_output"
        if not os.path.isdir(output_dir):
            os.mkdir(output_dir)
        os.chdir(output_dir)
        parse_file(FileParser(f))
