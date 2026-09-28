int counter = 0;

int add(int a, int b)
{
    int sum;

    sum = a + b;
    return sum;
}

int main(void)
{
    int i;
    int total = 0;

    for (i = 0; i < 5; i++) {
        total = add(total, i);
        counter++;
    }
    return total;
}
