// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <assert.h>
#include <myst/tee.h>
#include <openenclave/enclave.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, const char* argv[])
{
    if (strcmp(getenv("MYST_TARGET"), "sgx") == 0)
    {
        char line[1024];

        /* 1. Verify the private key conforms to pem format */
        FILE* fp = fopen("tmp/myst.key", "r");
        assert(fp != NULL);

        int linenum = 0;
        while (fgets(line, 1024, fp) != NULL)
        {
            printf("%s", line);
            if (linenum == 0)
                assert(strstr(line, "BEGIN RSA PRIVATE KEY"));
            linenum++;
        }
        fclose(fp);
        assert(linenum > 1);
        printf("Validated the private key file\n");

        /* 2. Verify the certificate was generated by Mystikos */
        fp = fopen("tmp/myst.crt", "r");
        assert(fp != NULL);
        int foundOE = 0;
        while (fgets(line, 1024, fp) != NULL)
        {
            if (strstr(line, "MYSTIKOS"))
            {
                foundOE = 1;
                break;
            }
        }
        fclose(fp);
        assert(foundOE != 0);
        printf("Validated the x509 certificate file\n");

        /* 3. Verify the report is valid and the identity looks correct. */
        fp = fopen("tmp/myst.report", "r");
        assert(fp != NULL);

        /* Read attestation report from file */
        char report[8192];
        size_t size = fread(report, 1, 8192, fp);
        assert(size < 8192);
        fclose(fp);

        oe_report_t parsed_report;
        uint64_t args[] = {
            (uint64_t)report, (uint64_t)size, (uint64_t)&parsed_report};
        int r = syscall(SYS_myst_oe_verify_report, args);

        assert(r == 0);
        assert(parsed_report.type == OE_ENCLAVE_TYPE_SGX);
        assert(parsed_report.identity.product_id[0] == 1);
        assert(parsed_report.identity.security_version == 1);

        printf("Report data: ");
        for (size_t i = 0; i < parsed_report.report_data_size; i++)
        {
            printf("%02x", parsed_report.report_data[i]);
        }
        printf("\n");

        printf("Validated the report file. Report size: %ld\n", size);
    }

    return 0;
}
