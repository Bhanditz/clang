// RUN: %clang_cc1 -verify -fopenmp %s
// RUN: %clang_cc1 -verify -fopenmp -std=c++98 %s
// RUN: %clang_cc1 -verify -fopenmp -std=c++11 %s

// RUN: %clang_cc1 -verify -fopenmp-simd %s
// RUN: %clang_cc1 -verify -fopenmp-simd -std=c++98 %s
// RUN: %clang_cc1 -verify -fopenmp-simd -std=c++11 %s

void foo() {
}

#if __cplusplus >= 201103L
// expected-note@+2 4 {{declared here}}
#endif
bool foobool(int argc) {
  return argc;
}

struct S1; // expected-note {{declared here}}

template <class T, typename S, int N, int ST> // expected-note {{declared here}}
T tmain(T argc, S **argv) {                   //expected-note 2 {{declared here}}
#pragma omp parallel for ordered
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered( // expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered() // expected-error {{expected expression}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
// expected-error@+3 {{expected ')'}} expected-note@+3 {{to match this '('}}
// expected-error@+2 2 {{expression is not an integral constant expression}}
// expected-note@+1 2 {{read of non-const variable 'argc' is not allowed in a constant expression}}
#pragma omp parallel for ordered(argc
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
// expected-error@+1 2 {{argument to 'ordered' clause must be a strictly positive integer value}}
#pragma omp parallel for ordered(ST // expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered(1)) // expected-warning {{extra tokens at the end of '#pragma omp parallel for' are ignored}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered((ST > 0) ? 1 + ST : 2) // expected-note 2 {{as specified in 'ordered' clause}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST]; // expected-error 2 {{expected 2 for loops after '#pragma omp parallel for', but found only 1}}
// expected-error@+6 2 {{directive '#pragma omp parallel for' cannot contain more than one 'ordered' clause}}
// expected-error@+5 2 {{argument to 'ordered' clause must be a strictly positive integer value}}
// expected-error@+4 2 {{expression is not an integral constant expression}}
#if __cplusplus >= 201103L
// expected-note@+2 2 {{non-constexpr function 'foobool' cannot be used in a constant expression}}
#endif
#pragma omp parallel for ordered(foobool(argc)), ordered(true), ordered(-5)
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered(S) // expected-error {{'S' does not refer to a value}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#if __cplusplus <= 199711L
// expected-error@+4 2 {{expression is not an integral constant expression}}
#else
// expected-error@+2 2 {{integral constant expression must have integral or unscoped enumeration type, not 'char *'}}
#endif
#pragma omp parallel for ordered(argv[1] = 2) // expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered(1)
  for (int i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered(N) // expected-error {{argument to 'ordered' clause must be a strictly positive integer value}}
  for (T i = ST; i < N; i++)
    argv[0][i] = argv[0][i] - argv[0][i - ST];
#pragma omp parallel for ordered(2) // expected-note {{as specified in 'ordered' clause}}
  foo();                            // expected-error {{expected 2 for loops after '#pragma omp parallel for'}}
  return argc;
}

int main(int argc, char **argv) {
#pragma omp parallel for ordered
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
#pragma omp parallel for ordered( // expected-error {{expected expression}} expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
#pragma omp parallel for ordered() // expected-error {{expected expression}}
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
#pragma omp parallel for ordered(4 // expected-error {{expected ')'}} expected-note {{to match this '('}} expected-note {{as specified in 'ordered' clause}}
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4]; // expected-error {{expected 4 for loops after '#pragma omp parallel for', but found only 1}}
#pragma omp parallel for ordered(2 + 2))      // expected-warning {{extra tokens at the end of '#pragma omp parallel for' are ignored}}  expected-note {{as specified in 'ordered' clause}}
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];            // expected-error {{expected 4 for loops after '#pragma omp parallel for', but found only 1}}
// expected-error@+4 {{expression is not an integral constant expression}}
#if __cplusplus >= 201103L
// expected-note@+2 {{non-constexpr function 'foobool' cannot be used in a constant expression}}
#endif
#pragma omp parallel for ordered(foobool(1) > 0 ? 1 : 2)
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
// expected-error@+6 {{expression is not an integral constant expression}}
#if __cplusplus >= 201103L
// expected-note@+4 {{non-constexpr function 'foobool' cannot be used in a constant expression}}
#endif
// expected-error@+2 2 {{directive '#pragma omp parallel for' cannot contain more than one 'ordered' clause}}
// expected-error@+1 2 {{argument to 'ordered' clause must be a strictly positive integer value}}
#pragma omp parallel for ordered(foobool(argc)), ordered(true), ordered(-5)
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
#pragma omp parallel for ordered(S1) // expected-error {{'S1' does not refer to a value}}
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
#if __cplusplus <= 199711L
// expected-error@+4 {{expression is not an integral constant expression}}
#else
// expected-error@+2 {{integral constant expression must have integral or unscoped enumeration type, not 'char *'}}
#endif
#pragma omp parallel for ordered(argv[1] = 2) // expected-error {{expected ')'}} expected-note {{to match this '('}}
  for (int i = 4; i < 12; i++)
    argv[0][i] = argv[0][i] - argv[0][i - 4];
// expected-error@+3 {{statement after '#pragma omp parallel for' must be a for loop}}
// expected-note@+1 {{in instantiation of function template specialization 'tmain<int, char, -1, -2>' requested here}}
#pragma omp parallel for ordered(ordered(tmain < int, char, -1, -2 > (argc, argv) // expected-error 2 {{expected ')'}} expected-note 2 {{to match this '('}}
  foo();
#pragma omp parallel for ordered(2) // expected-note {{as specified in 'ordered' clause}}
  foo();                            // expected-error {{expected 2 for loops after '#pragma omp parallel for'}}
  // expected-note@+1 {{in instantiation of function template specialization 'tmain<int, char, 1, 0>' requested here}}
  return tmain<int, char, 1, 0>(argc, argv);
}

