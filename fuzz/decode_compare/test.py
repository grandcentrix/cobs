from cobs import cobs


def process(input_data):
    # C expects the 0-byte, python doesn't
    if len(input_data) == 0:
        raise Exception("Missing 0-byte")

    if input_data[-1] != 0:
        raise Exception("last byte is not 0")

    input_data = input_data[:-1]

    return cobs.decode(input_data)
