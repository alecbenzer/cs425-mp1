class VectorTimestamp(list):

    def __eq__(a, b):
        return all(x == y for x, y in zip(a, b))

    def __ne__(a, b):
        return not (a == b)

    def __le__(a, b):
        return all(x <= y for x, y in zip(a, b))

    def __lt__(a, b):
        return a <= b and any(x < y for x, y in zip(a, b))

    def __ge__(a, b):
        return b <= a

    def __gt__(a, b):
        return b < a
